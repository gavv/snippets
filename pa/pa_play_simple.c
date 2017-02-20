/* Read decoded audio samples from stdin and send them to pulseaudio using simple API.
 * Input format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./pa_play_simple [sink_name] < cool_song_samples
 */

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/rtclock.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void print_info(
    pa_simple* simple,
    pa_sample_spec* sample_spec,
    pa_usec_t start_time,
    uint64_t n_bytes)
{
    const pa_usec_t position = pa_bytes_to_usec(n_bytes, sample_spec);

    const pa_usec_t timestamp = pa_rtclock_now() - start_time;

    const pa_usec_t latency = pa_simple_get_latency(simple, NULL);

    fprintf(stdout, "position=%lu ms, timestamp=%lu ms, diff=%ld ms, latency=%lu ms\n",
            (unsigned long)(position / 1000),
            (unsigned long)(timestamp / 1000),
            (((long)position - (long)timestamp) / 1000),
            (unsigned long)(latency / 1000));
}

int main(int argc, char **argv)
{
    if (argc != 1 && argc != 2) {
        fprintf(stderr, "usage: %s [sink_name] < input_file\n", argv[0]);
        exit(1);
    }

    const char *server_name = NULL;
    const char *client_name = "example play simple";
    const char *sink_name = NULL;
    const char *stream_name = "example stream";

    if (argc > 1) {
        sink_name = argv[1];
    }

    pa_sample_spec sample_spec = {};
    sample_spec.format = PA_SAMPLE_FLOAT32LE;
    sample_spec.rate = 44100;
    sample_spec.channels = 2;

    int error = 0;
    pa_simple *simple = pa_simple_new(
        server_name,
        client_name,
        PA_STREAM_PLAYBACK,
        sink_name,
        stream_name,
        &sample_spec,
        NULL,
        NULL,
        &error);

    if (simple == NULL) {
        fprintf(stderr, "pa_simple_new: %s\n", pa_strerror(error));
        exit(1);
    }

    const pa_usec_t start_time = pa_rtclock_now();
    uint64_t n_bytes = 0;

    for (;;) {
        char buf[1024];
        ssize_t sz = read(STDIN_FILENO, buf, sizeof(buf));
        if (sz == -1) {
            fprintf(stderr, "read: %s\n", strerror(errno));
            exit(1);
        }

        if (sz == 0) {
            break;
        }

        print_info(simple, &sample_spec, start_time, n_bytes);

        if (pa_simple_write(simple, buf, (size_t)sz, &error) != 0) {
            fprintf(stderr, "pa_simple_write: %s\n", pa_strerror(error));
            exit(1);
        }

        n_bytes += (uint64_t)sz;
    }

    /* wait until all samples are sent and played on server */
    if (pa_simple_drain(simple, &error) != 0) {
        fprintf(stderr, "pa_simple_drain: %s\n", pa_strerror(error));
        exit(1);
    }

    print_info(simple, &sample_spec, start_time, n_bytes);

    pa_simple_free(simple);

    return 0;
}
