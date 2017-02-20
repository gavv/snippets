/* Read decoded audio samples from stdin and send them to pulseaudio using async API
 * and callbacks.
 *
 * Input format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./pa_play_async_poll [latency_ms] [sink_name] < cool_song_samples
 */

#include <pulse/pulseaudio.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

struct userdata {
    pa_usec_t target_latency;

    const char *server_name;
    const char *client_name;
    const char *sink_name;
    const char *stream_name;

    pa_context *context;
    pa_stream *stream;
    pa_operation *drain;

    pa_usec_t start_time;
    bool eof;
    bool exit;
};

static void print_info(struct userdata* u)
{
    /* stream was not created yet */
    if (!u->stream) {
        return;
    }

    const pa_timing_info* timing_info = pa_stream_get_timing_info(u->stream);
    const pa_sample_spec* sample_spec = pa_stream_get_sample_spec(u->stream);

    /* timing info was not received yet */
    if (!timing_info || !sample_spec) {
        return;
    }

    pa_usec_t position = pa_bytes_to_usec(timing_info->write_index, sample_spec);
    pa_usec_t timestamp = pa_rtclock_now() - u->start_time;

    pa_usec_t latency = 0;
    int err;
    if ((err = pa_stream_get_latency(u->stream, &latency, NULL)) != 0) {
        fprintf(stderr, "pa_stream_get_latency: %s\n", pa_strerror(err));
        return;
    }

    fprintf(stdout, "position=%lu ms, timestamp=%lu ms, diff=%ld ms, latency=%lu ms\n",
            (unsigned long)(position / 1000),
            (unsigned long)(timestamp / 1000),
            (((long)position - (long)timestamp) / 1000),
            (unsigned long)(latency / 1000));
}

static void write_stream(struct userdata* u, size_t bufsz)
{
    if (u->eof) {
        return;
    }

    void *buf = NULL;

    /* request buffer from stream
     * we could also allocate it by ourselves, but this way buffering is zero-copy
     */
    int err;
    if ((err = pa_stream_begin_write(u->stream, &buf, &bufsz)) != 0) {
        fprintf(stderr, "pa_stream_begin_write: %s\n", pa_strerror(err));
        u->exit = true;
        return;
    }

    /* read samples from stdin
     * note that we block client mainloop if stdin is a pipe
     */
    ssize_t sz = read(STDIN_FILENO, buf, bufsz);
    if (sz < 0) {
        fprintf(stderr, "read: %s\n", strerror(errno));
        /* free stream buffer */
        pa_stream_cancel_write(u->stream);
        u->exit = true;
        return;
    }
    if (sz == 0) {
        /* free stream buffer */
        pa_stream_cancel_write(u->stream);
        u->eof = true;

        /* schedule stream drain
         * poll_stream() will check the operation state
         */
        if (!(u->drain = pa_stream_drain(u->stream, NULL, NULL))) {
            fprintf(stderr, "pa_stream_drain: %s\n",
                    pa_strerror(pa_context_errno(u->context)));
            u->exit = true;
        }

        return;
    }

    /* write samples to stream (non-blocking) */
    if ((err = pa_stream_write(u->stream, buf, sz, NULL, 0, PA_SEEK_RELATIVE)) != 0) {
        fprintf(stderr, "pa_stream_write: %s\n", pa_strerror(err));
        u->exit = true;
    }
}

static void start_stream(struct userdata *u)
{
    pa_sample_spec sample_spec = {};
    sample_spec.format = PA_SAMPLE_FLOAT32LE;
    sample_spec.rate = 44100;
    sample_spec.channels = 2;

    u->stream = pa_stream_new(u->context, u->stream_name, &sample_spec, NULL);
    if (u->stream == NULL) {
        fprintf(stderr, "pa_stream_new: %s\n",
                pa_strerror(pa_context_errno(u->context)));
        u->exit = true;
        return;
    }

    /*
     * server-side stream buffer parameters
     */
    pa_buffer_attr bufattr;
    /*
     * use maximum supported limit for server buffer size
     *
     * if pa_buffer_attr is null, defaults to `-1`
     */
    bufattr.maxlength = (uint32_t)-1;
    /*
     * set target latency
     *
     * positive value sets target latency (see PA_STREAM_ADJUST_LATENCY below)
     * -1 special value sets the highest latency the device supports
     *
     * if pa_buffer_attr is null, defaults to some smaller value
     * (so it differs from explicitly setting it to `-1`)
     */
    bufattr.tlength = pa_usec_to_bytes(u->target_latency, &sample_spec);
    /*
     * positive value delays playback start until first `prebuf` bytes are received
     * -1 special value sets `prebuf` equal to `tlength`
     *  0 special value disables automatic stream control (including draining)
     *
     * we want to start playback immediately after first server request, but we don't
     * want to disable draining, so we set it to `1`
     *
     * if pa_buffer_attr is null, defaults to `-1`
     */
    bufattr.prebuf = 1;
    /*
     * positive value defines minimum number of bytes that server requests from client
     * -1 special value allows server to choose some default value
     *
     * if pa_buffer_attr is null, defaults to `-1`
     */
    bufattr.minreq = (uint32_t)-1;

    int flags =
        /*
         * automatically update actual latency from server
         */
        PA_STREAM_AUTO_TIMING_UPDATE |
        /*
         * interpolate reported latency between updates
         */
        PA_STREAM_INTERPOLATE_TIMING |
        /*
         * without this flag, `tlength` is target size for the stream buffer.
         * pulseaudio will adjust stream buffer size to match `tlength`.
         *
         * with this flag, `tlength` is target size for the stream buffer plus output
         * sink buffer. pulseaudio will adjust stream buffer size to match
         * `tlength - slength`, where `slength` is the latency reported by the sink.
         *
         * note that pa_stream_get_latency() always reports the stream buffer size
         * (ignoring sink buffer size), so when this flag is set, its value is
         * lower than requested `tlength`.
         *
         * also note that the implementation of sink latency calculations depends
         * on the sink type.
         */
        PA_STREAM_ADJUST_LATENCY;

    int err = pa_stream_connect_playback(
        u->stream,
        u->sink_name,
        u->target_latency == 0 ? NULL : &bufattr,
        flags,
        NULL,
        NULL);
    if (err != 0) {
        fprintf(stderr, "pa_stream_connect_playback: %s\n", pa_strerror(err));
        u->exit = true;
        return;
    }

    u->start_time = pa_rtclock_now();
}

static void poll_stream(struct userdata *u)
{
    pa_stream_state_t state = pa_stream_get_state(u->stream);

    switch  (state) {
    case PA_STREAM_READY:
        /* stream is writable, proceed now */
        break;

    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
        /* stream is closed, exit */
        u->exit = true;
        return;

    default:
        /* stream is not ready yet */
        return;
    }

    print_info(u);

    /* if server requested more samples, send them */
    size_t sz = pa_stream_writable_size(u->stream);
    if (sz > 0) {
        write_stream(u, sz);
    }

    /* check if server finished playing all samples we've sent */
    if (u->drain) {
        if (pa_operation_get_state(u->drain) != PA_OPERATION_RUNNING) {
            u->exit = true;
        }
    }
}

static void poll_context(struct userdata *u)
{
    pa_context_state_t state = pa_context_get_state(u->context);

    switch  (state) {
    case PA_CONTEXT_READY:
        /* context connected to server, start playback stream */
        if (!u->stream) {
            start_stream(u);
        }
        break;

    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        /* context connection failed */
        u->exit = true;
        break;

    default:
        /* nothing interesting */
        break;
    }
}

static void run_mainloop(pa_mainloop *mainloop, struct userdata *u)
{
    /* create context (connection to server) */
    u->context = pa_context_new(pa_mainloop_get_api(mainloop), u->client_name);
    if (!u->context) {
        fprintf(stderr, "pa_context_new returned null\n");
        return;
    }

    /* schedule conenction to server */
    int err;
    if ((err = pa_context_connect(u->context, u->server_name, 0, NULL)) != 0) {
        fprintf(stderr, "pa_context_connect: %s\n", pa_strerror(err));
        return;
    }

    /* run mainloop until some callback sets `u->exit` */
    while (!u->exit) {
        /* run single mainloop iteration */
        if ((err = pa_mainloop_iterate(mainloop, 1, NULL)) < 0) {
            fprintf(stderr, "pa_mainloop_iterate: %s\n", pa_strerror(err));
            break;
        }

        poll_context(u);

        if (u->stream) {
            poll_stream(u);
        }
    }

    /* destroy drain operation */
    if (u->drain) {
        pa_operation_cancel(u->drain);
        pa_operation_unref(u->drain);
    }

    /* destroy stream */
    if (u->stream) {
        pa_stream_disconnect(u->stream);
        pa_stream_unref(u->stream);
    }

    /* destroy context */
    pa_context_disconnect(u->context);
    pa_context_unref(u->context);
}

int main(int argc, char **argv)
{
    if (argc > 3) {
        fprintf(stderr, "usage: %s [latency_ms] [sink_name] < input_file\n", argv[0]);
        exit(1);
    }

    struct userdata u = {};
    u.target_latency = 0; /* use defaults */
    u.server_name = NULL;
    u.client_name = "example play async poll";
    u.sink_name = NULL;
    u.stream_name = "example stream";

    if (argc > 1) {
        u.target_latency = atoi(argv[1]) * 1000;
    }

    if (argc > 2) {
        u.sink_name = argv[2];
    }

    pa_mainloop *mainloop = pa_mainloop_new();
    run_mainloop(mainloop, &u);
    pa_mainloop_free(mainloop);

    return 0;
}
