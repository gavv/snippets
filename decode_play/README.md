# Decoding and playing audio file in Linux

See details in [this article](https://gavv.github.io/articles/decode-play/).

### Overview

There are two types of snippets here:
* decoder: gets filename as argument, decodes audio stream from file, and writes decoded samples to stdout;
* player: reads decoded samples from stdin and sends them to the soundcard.

Decoded samples are always in the same format:
* linear PCM;
* two channels (front Left, front Right);
* interleaved format (L R L R ...);
* samples are 32-bits floats in little endian (actually CPU should be little-endian too);
* sample rate is 44100 Hz.

### Decoders

* `ffmpeg_decode` - decode file using [FFmpeg](https://www.ffmpeg.org/) (automatic resampling and channel mapping)
* `sox_decode_simple` - decode file using [SoX](http://sox.sourceforge.net/) (no resampling and channel mapping)
* `sox_decode_chain` - decode file using [SoX](http://sox.sourceforge.net/) (automatic resampling and channel mapping using effects chain)
* `sndfile_decode` - decode file using [libsndfile](http://www.mega-nerd.com/libsndfile/) (no resampling and channel mapping, only limited number of formats supported)

### Players

* `ffmpeg_play` - play decoded samples using [FFmpeg](https://www.ffmpeg.org/)
* `ffmpeg_play_encoder` - play decoded samples using [FFmpeg](https://www.ffmpeg.org/) (a bit more complex example demonstrating encoder usage)
* `sox_play` - play decoded samples using [SoX](http://sox.sourceforge.net/)
* `alsa_play_simple` - play decoded samples using `libasound` (with default parameters)
* `alsa_play_tuned` - play decoded samples using `libasound` (with customized parameters)

You can also find several implementations of [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/) client in [pulseaudio snippets](../pa) which use the same sample format.

### Building

```
$ make          # build all snippets
$ make clean    # cleanup
```

### Usage

Decoders and players may be connected via pipe. Any combinations are allowed, e.g.:

```
$ ./ffmpeg_decode     foo.mp3   |  ./alsa_play_tuned
$ ./sox_decode_chain  foo.mp3   |  ./ffmpeg_play
$ ./sndfile_decode    foo.flac  |  ./sox_play
```

You can also use `sox` and `play` utilities to generate or play samples:

```
$ sox -n -t f32 -r44100 -c2 - synth 30 sine 300 | ./alsa_play_tuned
$ ./ffmpeg_decode foo.mp3 | play -r44100 -c2 -t f32 -
```
