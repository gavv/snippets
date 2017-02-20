/* Read decoded audio samples from stdin and send them to ALSA using libasound.
 * Input format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./alsa_play_simple < cool_song_samples
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <alsa/asoundlib.h>

#define oops(func) (fprintf(stderr, "%s\n", func), exit(1))

static const int n_channels = 2, sample_rate = 44100;

int main(int argc, char** argv) {
    if (argc != 1) {
        fprintf(stderr, "usage: %s < input_file\n", argv[0]);
        exit(1);
    }

    snd_pcm_t* pcm = NULL;
    if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        oops("snd_pcm_open");
    }

    if (snd_pcm_set_params(pcm,
                           SND_PCM_FORMAT_FLOAT_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           n_channels,
                           sample_rate,
                           1,
                           sample_rate / 4) < 0) {
        oops("snd_pcm_set_params");
    }

    snd_pcm_uframes_t period_size = 0, buffer_size = 0;
    if (snd_pcm_get_params(pcm, &buffer_size, &period_size) < 0) {
        oops("snd_pcm_get_params");
    }

    printf("period_size = %ld\n", (long)period_size);
    printf("buffer_size = %ld\n", (long)buffer_size);

    const int buf_sz = period_size * n_channels * sizeof(float);
    void* buf = malloc(buf_sz);

    for (;;) {
        memset(buf, 0, buf_sz);

        const ssize_t rd_sz = read(STDIN_FILENO, buf, buf_sz);
        if (rd_sz < 0) {
            oops("read(stdin)");
        }

        if (rd_sz == 0) {
            break;
        }

        int ret = snd_pcm_writei(pcm, buf, period_size);

        if (ret < 0) {
            if ((ret = snd_pcm_recover(pcm, ret, 1)) == 0) {
                printf("recovered after xrun (overrun/underrun)\n");
            }
        }

        if (ret < 0) {
            oops("snd_pcm_writei");
        }
    }

    free(buf);

    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);

    return 0;
}
