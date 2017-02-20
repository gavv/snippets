/* Register pulseaudio source output which handles samples from source and
 * writes them to file.
 *
 * Output file format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   pactl load-module module-example-source-output \
 *      source=source_name \
 *      output_file=/path/to/file
 *
 *   pactl unload-module module-example-source-output
 */

/* config.h from pulseaudio directory (generated after ./configure) */
#include <config.h>

/* public pulseaudio headers */
#include <pulse/xmalloc.h>

/* private pulseaudio headers */
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/source-output.h>
#include <pulsecore/source.h>
#include <pulsecore/log.h>

#include <fcntl.h>
#include <errno.h>

PA_MODULE_AUTHOR("example author");
PA_MODULE_DESCRIPTION("example source output");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "source=<name for the source> "
        "output_file=<output file>");

struct example_source_output_userdata {
    pa_module *module;
    pa_source_output *source_output;

    const char *output_file;
    int output_fd;

    uint64_t n_bytes;
};

static const char* const example_source_output_modargs[] = {
    "source",
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
    struct example_source_output_userdata* u = PA_SOURCE_OUTPUT(o)->userdata;
    pa_assert(u);

    switch (code) {
    case PA_SOURCE_OUTPUT_MESSAGE_GET_LATENCY:
        /* if source output has additional internal latency, it may report
         * it here
         */
        *((pa_usec_t*)data) = 0;

        /* don't return, the default handler will add in the extra latency
         * added by the resampler
         */
        break;
    }

    return pa_source_output_process_msg(o, code, data, offset, chunk);
}

static void push_cb(pa_source_output *o, const pa_memchunk *chunk)
{
    pa_source_output_assert_ref(o);

    struct example_source_output_userdata* u = o->userdata;
    pa_assert(u);

    /* start reading chunk's memblock */
    const char *buf = pa_memblock_acquire(chunk->memblock);

    /* write samples from memblock to the file */
    ssize_t sz =
        write_samples(u->output_fd, buf + chunk->index, chunk->length);

    if (sz > 0) {
        u->n_bytes += sz;
    }

    /* finish reading memblock */
    pa_memblock_release(chunk->memblock);
}

static void kill_cb(pa_source_output* o)
{
    pa_source_output_assert_ref(o);

    struct example_source_output_userdata* u = o->userdata;
    pa_assert(u);

    pa_module_unload_request(u->module, true);

    pa_source_output_unlink(u->source_output);
    pa_source_output_unref(u->source_output);
    u->source_output = NULL;
}

void pa__done(pa_module*);

int pa__init(pa_module* m)
{
    pa_assert(m);

    /* this example uses fixed sample spec and channel map
     *
     * however, usually we to the following instead:
     *  - set sample spec and chennel map to default values:
     *      source->sample_spec
     *      source->channel_map
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
    if (!(args = pa_modargs_new(m->argument, example_source_output_modargs))) {
        pa_log("[example source output] failed to parse module arguments");
        goto error;
    }

    /* get source from arguments */
    pa_source *source = pa_namereg_get(
        m->core, pa_modargs_get_value(args, "source", NULL), PA_NAMEREG_SOURCE);
    if (!source) {
        pa_log("[example source output] source does not exist");
        goto error;
    }

    /* create and initialize module-specific data */
    struct example_source_output_userdata *u =
        pa_xnew0(struct example_source_output_userdata, 1);
    pa_assert(u);
    m->userdata = u;

    u->module = m;

    u->output_file = pa_modargs_get_value(args, "output_file", "/dev/null");
    u->output_fd = open(u->output_file, O_WRONLY | O_CREAT | O_TRUNC);
    if (u->output_fd == -1) {
        pa_log("[example source output] can't open output file %s", u->output_file);
        goto error;
    }

    /* create and initialize source output */
    pa_source_output_new_data data;
    pa_source_output_new_data_init(&data);
    pa_proplist_sets(data.proplist, PA_PROP_MEDIA_NAME, "example source output");
    pa_proplist_sets(data.proplist, "example_source_output_userdata.output_file",
                     u->output_file);
    data.driver = "example_source_output";
    data.module = m;
    pa_source_output_new_data_set_source(&data, source, false);
    pa_source_output_new_data_set_sample_spec(&data, &sample_spec);
    pa_source_output_new_data_set_channel_map(&data, &channel_map);

    pa_source_output_new(&u->source_output, m->core, &data);
    pa_source_output_new_data_done(&data);

    if (!u->source_output) {
        pa_log("[example source output] failed to create source output");
        goto error;
    }

    u->source_output->userdata = u;
    u->source_output->parent.process_msg = process_message;
    u->source_output->push = push_cb;
    u->source_output->kill = kill_cb;

    pa_source_output_put(u->source_output);
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

    struct example_source_output_userdata *u = m->userdata;
    if (!u) {
        return;
    }

    if (u->source_output) {
        pa_source_output_unlink(u->source_output);
        pa_source_output_unref(u->source_output);
    }

    if (u->output_fd != -1) {
        close(u->output_fd);
    }

    pa_xfree(u);
}
