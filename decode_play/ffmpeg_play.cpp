/* Read decoded audio samples from stdin and send them to ALSA using ffmpeg.
 * Input format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are little-endian 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./ffmpeg_play < cool_song_samples
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char** argv) {
    if (argc != 1) {
        fprintf(stderr, "usage: %s < input_file\n", argv[0]);
        exit(1);
    }

    const int in_channels = 2, in_samples = 512, sample_rate = 44100;
    const int bitrate = 64000;

    const int max_buffer_size =
        av_samples_get_buffer_size(
            NULL, in_channels, in_samples, AV_SAMPLE_FMT_FLT, 1);

    // register supported formats and codecs
    av_register_all();

    // register supported devices
    avdevice_register_all();

    // find output format for ALSA device
    AVOutputFormat* fmt = av_guess_format("alsa", NULL, NULL);
    if (!fmt) {
        fprintf(stderr, "av_guess_format()\n");
        exit(1);
    }

    // allocate empty format context
    // provides methods for writing output packets
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        fprintf(stderr, "avformat_alloc_context()\n");
        exit(1);
    }

    // tell format context to use ALSA as ouput device
    fmt_ctx->oformat = fmt;

    // add stream to format context
    AVStream* stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        fprintf(stderr, "avformat_new_stream()\n");
        exit(1);
    }

    // initialize stream codec context
    // format conetxt uses codec context when writing packets
    AVCodecContext* codec_ctx = stream->codec;
    assert(codec_ctx);
    codec_ctx->codec_id = AV_CODEC_ID_PCM_F32LE;
    codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLT;
    codec_ctx->bit_rate = bitrate;
    codec_ctx->sample_rate = sample_rate;
    codec_ctx->channels = in_channels;
    codec_ctx->channel_layout = AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT;

    // allocate buffer for input samples
    uint8_t* buffer = (uint8_t*)av_malloc(max_buffer_size);
    assert(buffer);

    // initialze output device
    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "avformat_write_header()\n");
        exit(1);
    }

    for (;;) {
        memset(buffer, 0, max_buffer_size);

        // read input buffer from stdin
        ssize_t ret = read(STDIN_FILENO, buffer, max_buffer_size);
        if (ret < 0) {
            fprintf(stderr, "read(stdin)\n");
            exit(1);
        }

        if (ret == 0) {
            break;
        }

        // create output packet
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = buffer;
        packet.size = max_buffer_size;

        // write output packet to format context
        if (av_write_frame(fmt_ctx, &packet) < 0) {
            fprintf(stderr, "av_write_frame()\n");
            exit(1);
        }
    }

    av_free(buffer);
    avformat_free_context(fmt_ctx);

    return 0;
}
