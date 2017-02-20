# Pulseaudio usage examples

### Overview

There are several types of snippets here:

* *Playback client*

    Standalone program that sends samples to pulseaudio.

* *Source*

    Pulseaudio module (shared library) that runs thread which implements timing and periodically generates samples and writes them to connected *source outputs*.

* *Source output*

    Pulseaudio module (shared library) that implements callbacks invoked when connected *source* generates more samples.

* *Sink*

    Pulseaudio module (shared library) that runs thread which implements timing and periodically requests samples from connected *sink inputs*.

* *Sink input*

    Pulseaudio module (shared library) that implements callbacks invoked when connected *sink* needs more samples.

When reading or writing samples to files, these snippets always use the same format:
* linear PCM;
* two channels (front Left, front Right);
* interleaved format (L R L R ...);
* samples are 32-bits floats in little endian (actually CPU should be little-endian too);
* sample rate is 44100 Hz.

See also [`decode_play`](../decode_play) snippets which demonstrate decoding and playing samples in this format using various media libraries.

### Snippets

* `pa_play_simple` - minimal playback client using [simple API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#simple_sec)

* `pa_play_async_cb` - playback client using [async API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#async_sec) and callbacks

* `pa_play_async_poll` - playback client using [async API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#async_sec) and polling

* `pa_module_source` - minimal pulseaudio source that maintains fixed latency

* `pa_module_source_output` - minimal pulseaudio source output

* `pa_module_sink_input` - minimal pulseaudio sink input

* `pa_module_sink` - minimal pulseaudio sink that maintains fixed latency

### Building

To build clients, install pulseaudio headers and run:

```
$ make
```

To build both clients and modules, download and build pulseauduio sources, and then run:

```
$ make PA_DIR=/path/to/pulseaudio/sources [PA_VER=x.y]
$ make PA_DIR=/path/to/pulseaudio/sources [PA_VER=x.y] install
```

It's recommended to build and load modules using the same version of pulseaudio.

Uninstall modules:

```
$ make uninstall
```

### Playback clients

Clients read samples from stdin and sends them to pulseaudio:

```
$ sox -n -t f32 -r44100 -c2 - synth 30 sine 300 | ./pa_play_simple
```

Some clients accept command line arguments to configure output sink and target latency, e.g:

```
$ ./pa_play_async_cb [latency_ms] [output_sink]
```

### Source

Generate sine and write it to `/tmp/input`:

```
$ sox -n -t f32 -r44100 -c2 - synth 30 sine 300 >/tmp/input
```

Register `example_source` reading samples from `/tmp/input`:

```
$ pactl load-module module-example-source input_file=/tmp/output
```

Register loopback device reading samples from `example_source` and sending it to default sink:

```
$ pactl load-module module-loopback source=example_source
```

### Source output

Register `my_null_sink`:

```
$ pactl load-module module-null-sink sink_name=my_null_sink
```

Register `example_source_output` connected to `my_null_sink.monitor` source and writing samples to `/tmp/output`:

```
$ touch /tmp/output
$ pactl load-module module-example-source-output \
    source=my_null_sink.monitor \
    output_file=/tmp/output
```

Generate sine and send it to `my_null_sink`:

```
$ sox -n -t f32 -r44100 -c2 - synth 30 sine 300 | ./pa_play_simple my_null_sink
```

Remove `example_source_output` and `my_null_sink`:

```
$ pactl unload-module module-example-source-output
$ pactl unload-module module-null-sink
```

Play written samples:

```
$ play -r44100 -c2 -t f32 /tmp/output
```

### Sink

Register `example_sink` writing samples to `/tmp/output`:

```
$ touch /tmp/output
$ pactl load-module module-example-sink output_file=/tmp/output
```

Generate sine and send it to `example_sink`:

```
$ sox -n -t f32 -r44100 -c2 - synth 30 sine 300 | ./pa_play_simple example_sink
```

Remove `example_sink`:

```
$ pactl unload-module module-example-sink
```

Play written samples:

```
$ play -r44100 -c2 -t f32 /tmp/output
```

### Sink input

Generate sine and write it to `/tmp/input`:

```
$ sox -n -t f32 -r44100 -c2 - synth 30 sine 300 >/tmp/input
```

Register `example_sink_input` connected to `Sink #123` and reading samples from input from `/tmp/input`:

```
$ pactl load-module module-example-sink-input sink=123 input_file=/tmp/output
```

You can get list of available sinks using `pactl list`. You can use both `sink=index` and `sink=name`.
