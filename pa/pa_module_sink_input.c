/* Register pulseaudio sink output which provides samples to sink by reading
 * them from file.
 *
 * Input file format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   pactl load-module module-example-sink-input \
 *      sink=sink_name \
 *      input_file=/path/to/file
 *
 *   pactl unload-module module-example-sink-output
 */

/* config.h from pulseaudio directory (generated after ./configure) */
#include <config.h>

/* public pulseaudio headers */
#include <pulse/xmalloc.h>

/* private pulseaudio headers */
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/log.h>

#include <fcntl.h>
#include <errno.h>

PA_MODULE_AUTHOR("example author");
PA_MODULE_DESCRIPTION("example sink input");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink=<name for the sink> "
        "input_file=<input file>");

struct example_sink_input_userdata {
    pa_module *module;
    pa_sink_input *sink_input;

    const char *input_file;
    int input_fd;
};

static const char* const example_sink_input_modargs[] = {
    "sink",
    "input_file",
    NULL
};

static ssize_t read_samples(int fd, char *buf, size_t bufsz)
{
    ssize_t len = read(fd, buf, bufsz);
    if (len < 0) {
        pa_log("[example sink input] read: %s", strerror(errno));
        return -1;
    }

    return len;
}

static int process_message(
    pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk)
{
    struct example_sink_input_userdata *u = PA_SINK_INPUT(o)->userdata;
    pa_assert(u);

    switch (code) {
    case PA_SINK_INPUT_MESSAGE_GET_LATENCY:
        /* if sink input has additional internal latency, it may report
         * it here
         */
        *((pa_usec_t*)data) = 0;

        /* don't return, the default handler will add in the extra latency
         * added by the resampler
         */
        break;
    }

    return pa_sink_input_process_msg(o, code, data, offset, chunk);
}

static int pop_cb(pa_sink_input *i, size_t length, pa_memchunk *chunk)
{
    pa_sink_input_assert_ref(i);

    struct example_sink_input_userdata* u = i->userdata;
    pa_assert(u);

    /* ensure that all chunk fields are set to zero */
    pa_memchunk_reset(chunk);

    /* allocate memblock */
    chunk->memblock = pa_memblock_new(u->module->core->mempool, length);

    /* start writing memblock */
    char *buf = pa_memblock_acquire(chunk->memblock);

    /* read samples from file to memblock */
    ssize_t sz =
        read_samples(u->input_fd, buf, length);

    /* finish writing memblock */
    pa_memblock_release(chunk->memblock);

    /* handle eof and error */
    if (sz <= 0) {
        /* this example plays single file and unloads itself */
        pa_module_unload_request(u->module, true);
        return -1;
    }

    /* setup chunk boundaries */
    chunk->index = 0;
    chunk->length = sz;

    return 0;
}

static void rewind_cb(pa_sink_input *i, size_t nbytes)
{
    pa_sink_input_assert_ref(i);

    struct example_sink_input_userdata* u = i->userdata;
    pa_assert(u);

    (void)nbytes;
}

static void kill_cb(pa_sink_input* i)
{
    pa_sink_input_assert_ref(i);

    struct example_sink_input_userdata* u = i->userdata;
    pa_assert(u);

    pa_module_unload_request(u->module, true);

    pa_sink_input_unlink(u->sink_input);
    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;
}

void pa__done(pa_module*);

int pa__init(pa_module* m)
{
    pa_assert(m);

    /* this example uses fixed sample spec and channel map
     *
     * however, usually we to the following instead:
     *  - set sample spec and chennel map to default values:
     *      sink->sample_spec
     *      sink->channel_map
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
    if (!(args = pa_modargs_new(m->argument, example_sink_input_modargs))) {
        pa_log("[example sink input] failed to parse module arguments");
        goto error;
    }

    /* get sink from arguments */
    pa_sink *sink = pa_namereg_get(
        m->core, pa_modargs_get_value(args, "sink", NULL), PA_NAMEREG_SINK);
    if (!sink) {
        pa_log("[example sink input] sink does not exist");
        goto error;
    }

    /* create and initialize module-specific data */
    struct example_sink_input_userdata *u =
        pa_xnew0(struct example_sink_input_userdata, 1);
    pa_assert(u);
    m->userdata = u;

    u->module = m;

    u->input_file = pa_modargs_get_value(args, "input_file", "/dev/zero");
    u->input_fd = open(u->input_file, O_RDONLY);
    if (u->input_fd == -1) {
        pa_log("[example sink input] can't open input file %s", u->input_file);
        goto error;
    }

    /* create and initialize sink input */
    pa_sink_input_new_data data;
    pa_sink_input_new_data_init(&data);
    pa_sink_input_new_data_set_sink(&data, sink, false);
    data.driver = "example_sink_input";
    data.module = u->module;
    pa_sink_input_new_data_set_sample_spec(&data, &sample_spec);
    pa_sink_input_new_data_set_channel_map(&data, &channel_map);

    pa_sink_input_new(&u->sink_input, u->module->core, &data);
    pa_sink_input_new_data_done(&data);

    if (!u->sink_input) {
        pa_log("[example sink input] failed to create sink input");
        goto error;
    }

    u->sink_input->userdata = u;
    u->sink_input->parent.process_msg = process_message;
    u->sink_input->pop = pop_cb;
    u->sink_input->process_rewind = rewind_cb;
    u->sink_input->kill = kill_cb;

    pa_sink_input_put(u->sink_input);
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

    struct example_sink_input_userdata *u = m->userdata;
    if (!u) {
        return;
    }

    if (u->sink_input) {
        pa_sink_input_unlink(u->sink_input);
        pa_sink_input_unref(u->sink_input);
    }

    if (u->input_fd != -1) {
        close(u->input_fd);
    }

    pa_xfree(u);
}
