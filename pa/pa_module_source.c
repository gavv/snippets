/* Register pulseaudio source which reads samples from file.
 *
 * Input file format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   pactl load-module module-example-source input_file=/path/to/file
 *   pactl unload-module module-example-source
 */

/* config.h from pulseaudio directory (generated after ./configure) */
#include <config.h>

/* public pulseaudio headers */
#include <pulse/xmalloc.h>
#include <pulse/rtclock.h>

/* private pulseaudio headers */
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/source.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/log.h>

#include <fcntl.h>
#include <errno.h>

PA_MODULE_AUTHOR("example author");
PA_MODULE_DESCRIPTION("example source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "source_name=<name for the source> "
        "source_properties=<properties for the source> "
        "input_file=<input file>");

struct example_source_userdata {
    pa_module *module;
    pa_source *source;

    pa_rtpoll *rtpoll;
    pa_thread *thread;
    pa_thread_mq thread_mq;

    const char *input_file;
    int input_fd;

    uint64_t posted_bytes;
};

static const char* const example_source_modargs[] = {
    "source_name",
    "source_properties",
    "input_file",
    NULL
};

static ssize_t read_samples(int fd, char *buf, size_t bufsz)
{
    ssize_t len = read(fd, buf, bufsz);
    if (len < 0) {
        pa_log("[example source] read: %s", strerror(errno));
        return -1;
    }

    return len;
}

static int process_message(
    pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk)
{
    switch (code) {
    case PA_SOURCE_MESSAGE_GET_LATENCY:
        /* if sink has additional internal latency, it may report it here */
        *((pa_usec_t*)data) = 0;
        return 0;
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

static void process_samples(struct example_source_userdata *u, uint64_t expected_bytes)
{
    pa_assert(u);

    if (expected_bytes <= u->posted_bytes) {
        return;
    }

    /* source should align chunks to sample size */
    size_t length =
        pa_frame_align(expected_bytes - u->posted_bytes, &u->source->sample_spec);

    if (length == 0) {
        return;
    }

    /* initialize chunk */
    pa_memchunk chunk;
    pa_memchunk_reset(&chunk);

    /* allocate memblock */
    chunk.memblock = pa_memblock_new(u->module->core->mempool, length);

    /* start writing memblock */
    char *buf = pa_memblock_acquire(chunk.memblock);

    /* read samples from file to memblock */
    ssize_t sz = read_samples(u->input_fd, buf, length);

    /* finish writing memblock */
    pa_memblock_release(chunk.memblock);

    /* handle eof and error */
    if (sz <= 0) {
        /* this example plays single file and unloads itself */
        pa_module_unload_request(u->module, true);
    }
    else {
        u->posted_bytes += sz;

        /* setup chunk bounds */
        chunk.index = 0;
        chunk.length = sz;

        /* send chunk to source outputs */
        pa_source_post(u->source, &chunk);
    }

    /* unref allocated memblock */
    pa_memblock_unref(chunk.memblock);
}

static void process_error(struct example_source_userdata *u)
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
    struct example_source_userdata *u = arg;
    pa_assert(u);

    pa_thread_mq_install(&u->thread_mq);

    const pa_usec_t poll_interval = 10000;

    pa_usec_t start_time = 0;
    pa_usec_t next_time = 0;

    for (;;) {
        /* generate samples */
        if (PA_SOURCE_IS_OPENED(u->source->thread_info.state)) {
            pa_usec_t now_time = pa_rtclock_now();

            if (start_time == 0) {
                start_time = now_time;
                next_time = start_time + poll_interval;
            }
            else {
                while (now_time >= next_time) {
                    uint64_t expected_bytes =
                        pa_usec_to_bytes(next_time - start_time, &u->source->sample_spec);

                    /* read samples from input file and write them to source outputs */
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
            pa_log("[example source] pa_rtpoll_run returned error");
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
    if (!(args = pa_modargs_new(m->argument, example_source_modargs))) {
        pa_log("[example source] failed to parse module arguments");
        goto error;
    }

    /* create and initialize module-specific data */
    struct example_source_userdata *u = pa_xnew0(struct example_source_userdata, 1);
    pa_assert(u);
    m->userdata = u;

    u->module = m;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    u->input_file = pa_modargs_get_value(args, "input_file", "/dev/zero");
    u->input_fd = open(u->input_file, O_RDONLY);
    if (u->input_fd == -1) {
        pa_log("[example source] can't open input file %s", u->input_file);
        goto error;
    }

    /* create and initialize source */
    pa_source_new_data data;
    pa_source_new_data_init(&data);
    data.driver = "example_source";
    data.module = m;
    pa_source_new_data_set_name(
        &data,
        pa_modargs_get_value(args, "source_name", "example_source"));
    pa_source_new_data_set_sample_spec(&data, &sample_spec);

    if (pa_modargs_get_proplist(
            args, "source_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("[example source] invalid source_properties");
        pa_source_new_data_done(&data);
        goto error;
    }

    u->source = pa_source_new(m->core, &data, PA_SOURCE_LATENCY);
    pa_source_new_data_done(&data);

    if (!u->source) {
        pa_log("[example source] failed to create source");
        goto error;
    }

    /* setup source callbacks */
    u->source->parent.process_msg = process_message;
    u->source->userdata = u;

    /* setup source event loop */
    pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
    pa_source_set_rtpoll(u->source, u->rtpoll);

    /* start thread for source event loop and sample generator */
    if (!(u->thread = pa_thread_new("example_source", thread_loop, u))) {
        pa_log("[example source] failed to create thread");
        goto error;
    }

    pa_source_put(u->source);
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

    struct example_source_userdata *u = m->userdata;
    if (!u) {
        return;
    }

    if (u->source) {
        pa_source_unlink(u->source);
    }

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->source) {
        pa_source_unref(u->source);
    }

    if (u->rtpoll) {
        pa_rtpoll_free(u->rtpoll);
    }

    if (u->input_fd != -1) {
        close(u->input_fd);
    }

    pa_xfree(u);
}
