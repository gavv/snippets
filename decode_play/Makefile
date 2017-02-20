snippets := \
	ffmpeg_decode \
	ffmpeg_play \
	ffmpeg_play_encoder \
	sox_decode_simple \
	sox_decode_chain \
	sox_play \
	sndfile_decode \
	alsa_play_simple \
	alsa_play_tuned

all: $(snippets)

clean:
	rm -f $(snippets)

ffmpeg_decode: ffmpeg_decode.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lavformat -lavcodec -lavutil -lswresample

ffmpeg_play: ffmpeg_play.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lavformat -lavcodec -lavdevice -lavutil

ffmpeg_play_encoder: ffmpeg_play_encoder.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lavformat -lavcodec -lavdevice -lavutil

sox_decode_simple: sox_decode_simple.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lsox

sox_decode_chain: sox_decode_chain.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lsox

sox_play: sox_play.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lsox

sndfile_decode: sndfile_decode.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lsndfile

alsa_play_simple: alsa_play_simple.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lasound

alsa_play_tuned: alsa_play_tuned.cpp Makefile
	g++ -ggdb -o $@ $@.cpp -lasound
