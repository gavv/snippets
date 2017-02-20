/* Read decoded audio samples from stdin and send them to ALSA using SoX.
 * Input format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./sox_play < cool_song_samples
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <sox.h>

#define oops(func) (fprintf(stderr, "%s\n", func), exit(1))

int main(int argc, char** argv) {
    if (argc != 1) {
        fprintf(stderr, "usage: %s < input_file\n", argv[0]);
        exit(1);
    }

    const int in_channels = 2, in_samples = 512, sample_rate = 44100;

    if (sox_init() != SOX_SUCCESS) {
        oops("sox_init()");
    }

    sox_signalinfo_t out_si = {};
    out_si.rate = sample_rate;
    out_si.channels = in_channels;
    out_si.precision = SOX_SAMPLE_PRECISION;

    sox_format_t* output
        = sox_open_write("default", &out_si, NULL, "alsa", NULL, NULL);
    if (!output) {
        oops("sox_open_read()");
    }

    sox_sample_t samples[in_samples * in_channels];

    float input[in_samples * in_channels];

    size_t clips = 0; SOX_SAMPLE_LOCALS;

    for (;;) {
        ssize_t sz = read(STDIN_FILENO, input, sizeof(input));
        if (sz < 0) {
            oops("read(stdin)");
        }
        if (sz == 0) {
            break;
        }

        const size_t n_samples = sz / sizeof(float);

        for (size_t n = 0; n < n_samples; n++) {
            samples[n] = SOX_FLOAT_32BIT_TO_SAMPLE(input[n], clips);
        }

        if (sox_write(output, samples, n_samples) != n_samples) {
            oops("sox_write()");
        }
    }

    if (sox_close(output) != SOX_SUCCESS) {
        oops("sox_close()");
    }

    if (sox_quit() != SOX_SUCCESS) {
        oops("sox_quit()");
    }

    return 0;
}
