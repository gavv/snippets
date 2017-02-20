/* Decode audio file using SoX and write decoded samples to stdout.
 * Output format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./sox_decode_simple cool_song.mp3 > cool_song_samples
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sox.h>

#define oops(func) (fprintf(stderr, "%s\n", func), exit(1))

static int stdout_writer(
    sox_effect_t* effect, const sox_sample_t* input, sox_sample_t* output,
    size_t* in_samples,
    size_t* out_samples)
{
    size_t clips = 0; SOX_SAMPLE_LOCALS;

    for (size_t pos = 0; pos < *in_samples; ) {
        size_t wr = 512;
        if (wr > (*in_samples - pos)) {
            wr = (*in_samples - pos);
        }

        float out_buf[wr];

        for (size_t n = 0; n < wr; n++) {
            out_buf[n] = SOX_SAMPLE_TO_FLOAT_32BIT(input[pos + n], clips);
        }

        const size_t out_bufsz = wr * sizeof(float);

        if (write(STDOUT_FILENO, out_buf, out_bufsz) != out_bufsz) {
            oops("write(stdout)");
        }

        pos += wr;
    }

    *out_samples = 0;

    return SOX_SUCCESS;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file > output_file\n", argv[0]);
        exit(1);
    }

    const int out_channels = 2, sample_rate = 44100;

    if (sox_init() != SOX_SUCCESS) {
        oops("sox_init()");
    }

    sox_format_t* input = sox_open_read(argv[1], NULL, NULL, NULL);
    if (!input) {
        oops("sox_open_read()");
    }

    sox_signalinfo_t out_si = {};
    out_si.rate = sample_rate;
    out_si.channels = out_channels;
    out_si.precision = SOX_SAMPLE_PRECISION;

    sox_effect_handler_t out_handler = {
        "stdout", NULL, SOX_EFF_MCHAN, NULL, NULL, stdout_writer, NULL, NULL, NULL, 0
    };

    sox_effects_chain_t* chain
        = sox_create_effects_chain(&input->encoding, NULL);
    if (!chain) {
        oops("sox_create_effects_chain()");
    }

    {
        sox_effect_t* effect = sox_create_effect(sox_find_effect("input"));
        if (!effect) {
            oops("sox_create_effect(input)");
        }

        char* args[1] = { (char*)input };
        if (sox_effect_options(effect, 1, args) != SOX_SUCCESS) {
            oops("sox_effect_options(input)");
        }

        if (sox_add_effect(
                chain, effect, &input->signal, &out_si) != SOX_SUCCESS) {
            oops("sox_add_effect(input)");
        }

        free(effect);
    }

    if (input->signal.rate != out_si.rate) {
        {
            sox_effect_t* effect = sox_create_effect(sox_find_effect("gain"));
            if (!effect) {
                oops("sox_create_effect(gain)");
            }

            const char* args[] = { "-h" };

            if (sox_effect_options(effect, 1, (char**)args) != SOX_SUCCESS) {
                oops("sox_effect_options(gain)");
            }

            if (sox_add_effect(
                    chain, effect, &input->signal, &out_si) != SOX_SUCCESS) {
                oops("sox_add_effect(gain)");
            }

            free(effect);
        }

        {
            sox_effect_t* effect = sox_create_effect(sox_find_effect("rate"));
            if (!effect) {
                oops("sox_create_effect(rate)");
            }

            const char* args[] = { "-Q", "7", "-b", "99.7" };

            if (sox_effect_options(effect, 4, (char**)args) != SOX_SUCCESS) {
                oops("sox_effect_options(rate)");
            }

            if (sox_add_effect(
                    chain, effect, &input->signal, &out_si) != SOX_SUCCESS) {
                oops("sox_add_effect(rate)");
            }

            free(effect);
        }
    }

    if (input->signal.channels != out_si.channels) {
        sox_effect_t* effect = sox_create_effect(sox_find_effect("channels"));
        if (!effect) {
            oops("sox_create_effect(channels)");
        }

        if (sox_effect_options(effect, 0, NULL) != SOX_SUCCESS) {
            oops("sox_effect_options(channels)");
        }

        if (sox_add_effect(
                chain, effect, &input->signal, &out_si) != SOX_SUCCESS) {
            oops("sox_add_effect(channels)");
        }

        free(effect);
    }

    {
        sox_effect_t* effect = sox_create_effect(&out_handler);
        if (!effect) {
            oops("sox_create_effect(output)");
        }

        if (sox_add_effect(
                chain, effect, &input->signal, &out_si) != SOX_SUCCESS) {
            oops("sox_add_effect(output)");
        }

        free(effect);
    }

    sox_flow_effects(chain, NULL, NULL);

    sox_delete_effects_chain(chain);

    if (sox_close(input) != SOX_SUCCESS) {
        oops("sox_close()");
    }

    if (sox_quit() != SOX_SUCCESS) {
        oops("sox_quit()");
    }

    return 0;
}
