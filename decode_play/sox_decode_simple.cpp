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

#include <sox.h>

#define oops(func) (fprintf(stderr, "%s\n", func), exit(1))

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file > output_file\n", argv[0]);
        exit(1);
    }

    const int out_channels = 2, out_samples = 512, sample_rate = 44100;

    if (sox_init() != SOX_SUCCESS) {
        oops("sox_init()");
    }

    sox_format_t* input = sox_open_read(argv[1], NULL, NULL, NULL);
    if (!input) {
        oops("sox_open_read()");
    }

    if (input->signal.rate != sample_rate) {
        oops("unsupported sample rate");
    }

    if (input->signal.channels != out_channels) {
        oops("unsupported # of channels");
    }

    sox_sample_t buf[out_samples * out_channels];

    float out[out_samples * out_channels];

    size_t clips = 0; SOX_SAMPLE_LOCALS;

    for (;;) {
        size_t sz = sox_read(input, buf, out_samples * out_channels);
        if (sz == 0) {
            break;
        }

        for (size_t n = 0; n < sz; n++) {
            out[n] = SOX_SAMPLE_TO_FLOAT_32BIT(buf[n], clips);
        }

        const size_t out_sz = sz * sizeof(float);

        if (write(STDOUT_FILENO, out, out_sz) != out_sz) {
            oops("write(stdout)");
        }
    }

    if (sox_close(input) != SOX_SUCCESS) {
        oops("sox_close()");
    }

    if (sox_quit() != SOX_SUCCESS) {
        oops("sox_quit()");
    }

    return 0;
}
