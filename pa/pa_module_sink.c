/* Register pulseaudio sink which writes samples to file and maintains fixed latency.
 *
 * Output file format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   pactl load-module module-example-sink output_file=/path/to/file
 *   pactl unload-module module-example-sink
 */

/* config.h from pulseaudio directory (generated after ./configure) */
#include <config.h>

/* public pulseaudio headers */
#include <pulse/xmalloc.h>
#include <pulse/rtclock.h>

/* private pulseaudio headers */
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/sink.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/log.h>

#include <fcntl.h>
#include <errno.h>

PA_MODULE_AUTHOR("example author");
PA_MODULE_DESCRIPTION("example sink");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink_name=<name for the sink> "
        "sink_properties=<properties for the sink> "
        "output_file=<output file>");

struct example_sink_userdata {
    pa_module *module;
    pa_sink *sink;

    pa_rtpoll *rtpoll;
    pa_thread *thread;
    pa_thread_mq thread_mq;

    const char *output_file;
    int output_fd;

    uint64_t rendered_bytes;
};

static const char* const example_sink_modargs[] = {
    "sink_name",
    "sink_properties",
    "output_file",
    NULL
};

static ssize_t write_samples(int fd, const char *buf, size_t bufsz)
{
    size_t off = 0;
    while (off < bufsz) {
        ssize_t ret = write(fd, buf + off, bufsz - off);
        if (ret == -1) {
            pa_log("[example sink] write: %s", strerror(errno));
            return -1;
        }

        off += (size_t)ret;
    }

    return (ssize_t)bufsz;
}

static int process_message(
    pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk)
{
    switch (code) {
    case PA_SINK_MESSAGE_GET_LATENCY:
        /* if sink has additional internal latency, it may report it here */
        *((pa_usec_t*)data) = 0;
        return 0;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static void process_samples(struct example_sink_userdata *u, uint64_t expected_bytes)
{
    pa_assert(u);

    while (u->rendered_bytes < expected_bytes) {
        /* read chunk from every connected sink input, mix them, allocate
         * memblock, fill it with mixed samples, and return it to us.
         */
        pa_memchunk chunk;
        pa_sink_render(u->sink, 0, &chunk);

        /* start reading chunk's memblock */
        const char *buf = pa_memblock_acquire(chunk.memblock);

        /* write samples from memblock to the file */
        ssize_t sz =
            write_samples(u->output_fd, buf + chunk.index, chunk.length);

        if (sz != chunk.length) {
            break;
        }

        u->rendered_bytes += chunk.length;

        /* finish reading memblock */
        pa_memblock_release(chunk.memblock);

        /* return memblock to the pool */
        pa_memblock_unref(chunk.memblock);
    }
}

static void process_rewind(struct example_sink_userdata *u)
{
    pa_assert(u);

    pa_sink_process_rewind(u->sink, 0);
}

static void process_error(struct example_sink_userdata *u)
{
    pa_assert(u);

    pa_asyncmsgq_post(
        u->thread_mq.outq,
        PA_MSGOBJECT(u->module->core),
        PA_CORE_MESSAGE_UNLOAD_MODULE,
        u->module,
        0,
        NULL,
        NULL);

    pa_asyncmsgq_wait_for(
        u->thread_mq.inq,
        PA_MESSAGE_SHUTDOWN);
}

static void thread_loop(void *arg)
{
    struct example_sink_userdata *u = arg;
    pa_assert(u);

    pa_thread_mq_install(&u->thread_mq);

    const pa_usec_t poll_interval = 10000;

    pa_usec_t start_time = 0;
    pa_usec_t next_time = 0;

    for (;;) {
        /* process rewind */
        if (u->sink->thread_info.rewind_requested) {
            process_rewind(u);
        }

        /* process sink inputs */
        if (PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
            pa_usec_t now_time = pa_rtclock_now();

            if (start_time == 0) {
                start_time = now_time;
                next_time = start_time + poll_interval;
            }
            else {
                while (now_time >= next_time) {
                    uint64_t expected_bytes =
                        pa_usec_to_bytes(next_time - start_time, &u->sink->sample_spec);

                    /* render samples from sink inputs and write them to output file */
                    process_samples(u, expected_bytes);

                    /* next tick */
                    next_time += poll_interval;
                }
            }

            /* schedule set next rendering tick */
            pa_rtpoll_set_timer_absolute(u->rtpoll, next_time);
        }
        else {
            /* sleep until state change */
            start_time = 0;
            next_time = 0;
            pa_rtpoll_set_timer_disabled(u->rtpoll);
        }

        /* process events and wait next rendering tick */
#if PA_CHECK_VERSION(5, 99, 0)
        int ret = pa_rtpoll_run(u->rtpoll);
#else
        int ret = pa_rtpoll_run(u->rtpoll, true);
#endif
        if (ret < 0) {
            pa_log("[example sink] pa_rtpoll_run returned error");
            goto error;
        }

        if (ret == 0) {
            break;
        }
    }

    return;

error:
    process_error(u);
}

void pa__done(pa_module*);

int pa__init(pa_module* m)
{
    pa_assert(m);

    /* this example uses fixed sample spec and channel map
     *
     * however, usually we to the following instead:
     *  - set sample spec and chennel map to default values:
     *      m->core->default_sample_spec
     *      m->core->default_channel_map
     *
     *  - overwrite them with pa_modargs_get_sample_spec_and_channel_map()
     *    if module was loaded with corresponding arguments
     *
     *  - finally adjust values to nearest supported form
     */
    pa_sample_spec sample_spec;
    sample_spec.format = PA_SAMPLE_FLOAT32LE;
    sample_spec.rate = 44100;
    sample_spec.channels = 2;

    pa_channel_map channel_map;
    pa_channel_map_init_stereo(&channel_map);

    /* get module arguments (key-value list passed to load-module) */
    pa_modargs *args;
    if (!(args = pa_modargs_new(m->argument, example_sink_modargs))) {
        pa_log("[example sink] failed to parse module arguments");
        goto error;
    }

    /* create and initialize module-specific data */
    struct example_sink_userdata *u = pa_xnew0(struct example_sink_userdata, 1);
    pa_assert(u);
    m->userdata = u;

    u->module = m;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    u->output_file = pa_modargs_get_value(args, "output_file", "/dev/null");
    u->output_fd = open(u->output_file, O_WRONLY | O_CREAT | O_TRUNC);
    if (u->output_fd == -1) {
        pa_log("[example sink] can't open output file %s", u->output_file);
        goto error;
    }

    /* create and initialize sink */
    pa_sink_new_data data;
    pa_sink_new_data_init(&data);
    data.driver = "example_sink";
    data.module = m;
    pa_sink_new_data_set_name(
        &data,
        pa_modargs_get_value(args, "sink_name", "example_sink"));
    pa_sink_new_data_set_sample_spec(&data, &sample_spec);
    pa_sink_new_data_set_channel_map(&data, &channel_map);

    if (pa_modargs_get_proplist(
            args,
            "sink_properties",
            data.proplist,
            PA_UPDATE_REPLACE) < 0) {
        pa_log("[example sink] invalid sink properties");
        pa_sink_new_data_done(&data);
        goto error;
    }

    u->sink = pa_sink_new(m->core, &data, PA_SINK_LATENCY);
    pa_sink_new_data_done(&data);

    if (!u->sink) {
        pa_log("[example sink] failed to create sink");
        goto error;
    }

    /* setup sink callbacks */
    u->sink->parent.process_msg = process_message;
    u->sink->userdata = u;

    /* setup sink event loop */
    pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
    pa_sink_set_rtpoll(u->sink, u->rtpoll);

    /* start thread for sink event loop and sample reader */
    if (!(u->thread = pa_thread_new("example_sink", thread_loop, u))) {
        pa_log("[example sink] failed to create thread");
        goto error;
    }

    pa_sink_put(u->sink);
    pa_modargs_free(args);

    return 0;

error:
    if (args) {
        pa_modargs_free(args);
    }
    pa__done(m);

    return -1;
}

void pa__done(pa_module *m)
{
    pa_assert(m);

    struct example_sink_userdata *u = m->userdata;
    if (!u) {
        return;
    }

    if (u->sink) {
        pa_sink_unlink(u->sink);
    }

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->sink) {
        pa_sink_unref(u->sink);
    }

    if (u->rtpoll) {
        pa_rtpoll_free(u->rtpoll);
    }

    if (u->output_fd != -1) {
        close(u->output_fd);
    }

    pa_xfree(u);
}
