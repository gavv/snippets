# PulseAudio usage examples

See these articles: [1](https://gavv.github.io/articles/pulseaudio-under-the-hood/), [2](https://gavv.github.io/articles/decode-play/).

### Overview

There are several types of snippets here:

* **D-Bus client**

    Standalone program that uses PulseAudio D-Bus API.

* **Recording client**

    Standalone program that uses PulseAudio C API to receive samples from the server.

* **Playback client**

    Standalone program that uses PulseAudio C API to send samples to the server.

* **Source**

    PulseAudio module (shared library) that runs a thread which implements timing and periodically generates samples and writes them to the connected source outputs.

* **Source output**

    PulseAudio module (shared library) that implements callbacks invoked when a source generates more samples.

* **Sink**

    PulseAudio module (shared library) that runs a thread which implements timing and periodically requests samples from the connected sink inputs.

* **Sink input**

    PulseAudio module (shared library) that implements callbacks invoked when a sink needs more samples.

### Sample format

When reading or writing samples to files, these snippets always use the same format:

* linear PCM;
* two channels (front Left, front Right);
* interleaved format (L R L R ...);
* samples are 32-bits floats in little endian (actually CPU should be little-endian too);
* sample rate is 44100 Hz.

See also [`decode_play`](../decode_play) snippets which demonstrate decoding and playing samples in this format using various media libraries.

### Snippets

* `pa_dbus_print` - Python3 script that prints various server-side objects using the [D-Bus API](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/DBus/)

* `pa_record_simple` - minimal recording client using the [simple API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#simple_sec)

* `pa_play_simple` - minimal playback client using the [simple API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#simple_sec)

* `pa_play_async_cb` - playback client using the [async API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#async_sec) and callbacks

* `pa_play_async_poll` - playback client using the [async API](https://freedesktop.org/software/pulseaudio/doxygen/index.html#async_sec) and polling

* `pa_module_source` - minimal PulseAudio source that maintains fixed latency

* `pa_module_source_output` - minimal PulseAudio source output

* `pa_module_sink_input` - minimal PulseAudio sink input

* `pa_module_sink` - minimal PulseAudio sink that maintains fixed latency

### Building

To build clients, install PulseAudio headers and run:

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

### D-Bus clients

Enable D-Bus API:

```
$ pactl load-module module-dbus-protocol
```

Print server-side objects:

```
$ ./pa_dbus_print.py
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
