/* Decode audio file using ffmpeg and write decoded samples to stdout.
 * Output format:
 *  - two channels (front left, front right)
 *  - samples in interleaved format (L R L R ...)
 *  - samples are 32-bit floats
 *  - sample rate is 44100
 *
 * Usage:
 *   ./ffmpeg_decode cool_song.mp3 > cool_song_samples
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file > output_file\n", argv[0]);
        exit(1);
    }

    const int out_channels = 2, out_samples = 512, sample_rate = 44100;

    const int max_buffer_size =
        av_samples_get_buffer_size(
            NULL, out_channels, out_samples, AV_SAMPLE_FMT_FLT, 1);

    // register supported formats and codecs
    av_register_all();

    // allocate empty format context
    // provides methods for reading input packets
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    assert(fmt_ctx);

    // determine input file type and initialize format context
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) != 0) {
        fprintf(stderr, "error: avformat_open_input()\n");
        exit(1);
    }

    // determine supported codecs for input file streams and add
    // them to format context
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "error: avformat_find_stream_info()\n");
        exit(1);
    }

#if 0
    av_dump_format(fmt_ctx, 0, argv[1], false);
#endif

    // find audio stream in format context
    size_t stream = 0;
    for (; stream < fmt_ctx->nb_streams; stream++) {
        if (fmt_ctx->streams[stream]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            break;
        }
    }
    if (stream == fmt_ctx->nb_streams) {
        fprintf(stderr, "error: no audio stream found\n");
        exit(1);
    }

    // get codec context for audio stream
    // provides methods for decoding input packets received from format context
    AVCodecContext* codec_ctx = fmt_ctx->streams[stream]->codec;
    assert(codec_ctx);

    if (codec_ctx->channel_layout == 0) {
        codec_ctx->channel_layout = AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT;
    }

    // find decoder for audio stream
    AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
    if (!codec) {
        fprintf(stderr, "error: avcodec_find_decoder()\n");
        exit(1);
    }

    // initialize codec context with decoder we've found
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "error: avcodec_open2()\n");
        exit(1);
    }

    // initialize converter from input audio stream to output stream
    // provides methods for converting decoded packets to output stream
    SwrContext* swr_ctx =
        swr_alloc_set_opts(NULL,
                           AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT, // output
                           AV_SAMPLE_FMT_FLT,                    // output
                           sample_rate,                          // output
                           codec_ctx->channel_layout,  // input
                           codec_ctx->sample_fmt,      // input
                           codec_ctx->sample_rate,     // input
                           0,
                           NULL);
    if (!swr_ctx) {
        fprintf(stderr, "error: swr_alloc_set_opts()\n");
        exit(1);
    }
    swr_init(swr_ctx);

    // create empty packet for input stream
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    // allocate empty frame for decoding
    AVFrame* frame = av_frame_alloc();
    assert(frame);

    // allocate buffer for output stream
    uint8_t* buffer = (uint8_t*)av_malloc(max_buffer_size);
    assert(buffer);

    // read packet from input audio file
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        // skip non-audio packets
        if (packet.stream_index != stream) {
            continue;
        }

        // decode packet to frame
        int got_frame = 0;
        if (avcodec_decode_audio4(codec_ctx, frame, &got_frame, &packet) < 0) {
            fprintf(stderr, "error: avcodec_decode_audio4()\n");
            exit(1);
        }

        if (!got_frame) {
            continue;
        }

        // convert input frame to output buffer
        int got_samples = swr_convert(
            swr_ctx,
            &buffer, out_samples,
            (const uint8_t **)frame->data, frame->nb_samples);

        if (got_samples < 0) {
            fprintf(stderr, "error: swr_convert()\n");
            exit(1);
        }

        while (got_samples > 0) {
            int buffer_size =
                av_samples_get_buffer_size(
                    NULL, out_channels, got_samples, AV_SAMPLE_FMT_FLT, 1);

            assert(buffer_size <= max_buffer_size);

            // write output buffer to stdout
            if (write(STDOUT_FILENO, buffer, buffer_size) != buffer_size) {
                fprintf(stderr, "error: write(stdout)\n");
                exit(1);
            }

            // process samples buffered inside swr context
            got_samples = swr_convert(swr_ctx, &buffer, out_samples, NULL, 0);
            if (got_samples < 0) {
                fprintf(stderr, "error: swr_convert()\n");
                exit(1);
            }
        }

        // free packet created by decoder
        av_free_packet(&packet);
    }

    av_free(buffer);
    av_frame_free(&frame);

    swr_free(&swr_ctx);

    avcodec_close(codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}
