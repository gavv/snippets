/* Read decoded audio samples from stdin and send them to ALSA using libasound.
 * Input format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./alsa_play_tuned < cool_song_samples
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <alsa/asoundlib.h>

#define oops(func) (fprintf(stderr, "%s\n", func), exit(1))

static const int n_channels = 2, sample_rate = 44100;

void set_hw_params(snd_pcm_t* pcm,
                   snd_pcm_uframes_t* period_size, snd_pcm_uframes_t* buffer_size) {
    //
    snd_pcm_hw_params_t* hw_params = NULL;
    snd_pcm_hw_params_alloca(&hw_params);

    // initialize hw_params
    if (snd_pcm_hw_params_any(pcm, hw_params) < 0) {
        oops("snd_pcm_hw_params_any");
    }

    // enable software resampling
    if (snd_pcm_hw_params_set_rate_resample(pcm, hw_params, 1) < 0) {
        oops("snd_pcm_hw_params_set_rate_resample");
    }

    // set number of channels
    if (snd_pcm_hw_params_set_channels(pcm, hw_params, n_channels) < 0) {
        oops("snd_pcm_hw_params_set_channels");
    }

    // set interleaved format (L R L R ...)
    if (int ret =
        snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        oops("snd_pcm_hw_params_set_access");
    }

    // set little endian 32-bit floats
    if (snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_FLOAT_LE) < 0) {
        oops("snd_pcm_hw_params_set_format");
    }

    // set sample rate
    unsigned int rate = sample_rate;
    if (snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, 0) < 0) {
        oops("snd_pcm_hw_params_set_rate_near");
    }
    if (rate != sample_rate) {
        oops("can't set sample rate (exact value is not supported)");
    }

    // set period time in microseconds
    // ALSA reads 'period_size' samples from circular buffer every period
    unsigned int period_time = sample_rate / 4;
    if (int ret =
        snd_pcm_hw_params_set_period_time_near(pcm, hw_params, &period_time, NULL) < 0) {
        oops("snd_pcm_hw_params_set_period_time_near");
    }

    // get period size, i.e. number of samples fetched from circular buffer
    // every period, calculated from 'sample_rate' and 'period_time'
    *period_size = 0;
    if (snd_pcm_hw_params_get_period_size(hw_params, period_size, NULL) < 0) {
        oops("snd_pcm_hw_params_get_period_size");
    }

    // set buffer size, i.e. number of samples in circular buffer
    *buffer_size = *period_size * 8;
    if (snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, buffer_size) < 0) {
        oops("snd_pcm_hw_params_set_buffer_size_near");
    }

    // get buffer time, i.e. total duration of circular buffer in microseconds,
    // calculated from 'sample_rate' and 'buffer_size'
    unsigned int buffer_time = 0;
    if (snd_pcm_hw_params_get_buffer_time(hw_params, &buffer_time, NULL) < 0) {
        oops("snd_pcm_hw_params_get_buffer_time");
    }

    printf("period_size = %ld\n", (long)*period_size);
    printf("period_time = %ld\n", (long)period_time);
    printf("buffer_size = %ld\n", (long)*buffer_size);
    printf("buffer_time = %ld\n", (long)buffer_time);

    // send hw_params to ALSA
    if (snd_pcm_hw_params(pcm, hw_params) < 0) {
        oops("snd_pcm_hw_params");
    }
}

void set_sw_params(snd_pcm_t* pcm,
                   snd_pcm_uframes_t period_size, snd_pcm_uframes_t buffer_size) {
    //
    snd_pcm_sw_params_t* sw_params = NULL;
    snd_pcm_sw_params_alloca(&sw_params);

    // initialize sw_params
    if (snd_pcm_sw_params_current(pcm, sw_params) < 0) {
        oops("snd_pcm_sw_params_current");
    }

    // set start threshold to buffer_size, so that ALSA starts playback only
    // after circular buffer becomes full first time
    if (snd_pcm_sw_params_set_start_threshold(pcm, sw_params, buffer_size) < 0) {
        oops("snd_pcm_sw_params_set_start_threshold");
    }

    // set minimum number of samples that can be read by ALSA, so that it'll
    // wait until there are at least 'period_size' samples in circular buffer
    if (snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size) < 0) {
        oops("snd_pcm_sw_params_set_avail_min");
    }

    // send sw_params to ALSA
    if (snd_pcm_sw_params(pcm, sw_params) < 0) {
        oops("snd_pcm_sw_params");
    }
}

int main(int argc, char** argv) {
    if (argc != 1) {
        fprintf(stderr, "usage: %s < input_file\n", argv[0]);
        exit(1);
    }

    snd_pcm_t* pcm = NULL;
    if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        oops("snd_pcm_open");
    }

    snd_pcm_uframes_t period_size = 0, buffer_size = 0;
    set_hw_params(pcm, &period_size, &buffer_size);
    set_sw_params(pcm, period_size, buffer_size);

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
