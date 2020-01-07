#
CC := gcc
CFLAGS := -ggdb -Wall

PA_VER ?= $(shell pulseaudio --version | cut -d' ' -f2)

PA_MOD_DIR ?= \
	$(shell pulseaudio --dump-conf | grep dl-search-path | awk '{print $$3}')

PA_MOD_FLAGS ?= \
	-shared \
	-fPIC \
	-I$(PA_DIR) \
	-I$(PA_DIR)/src \
	-L$(PA_DIR)/src/.libs \
	-L/usr/lib/pulseaudio \
	-lpulsecore-$(PA_VER) \
	-lpulsecommon-$(PA_VER) \
	-lpulse

CLIENTS := \
	pa_play_simple \
	pa_play_async_cb \
	pa_play_async_poll

MODULES := \
	module-example-source.so \
	module-example-source-output.so \
	module-example-sink-input.so \
	module-example-sink.so

ifeq ($(PA_DIR),)
BINARIES := $(CLIENTS)
else
BINARIES := $(CLIENTS) $(MODULES)
endif

all: $(BINARIES)

install: $(MODULES)
	ln -sf $(shell pwd)/module-example-source.so \
		$(PA_MOD_DIR)/module-example-source.so
	ln -sf $(shell pwd)/module-example-source-output.so \
		$(PA_MOD_DIR)/module-example-source-output.so
	ln -sf $(shell pwd)/module-example-sink-input.so \
		$(PA_MOD_DIR)/module-example-sink-input.so
	ln -sf $(shell pwd)/module-example-sink.so \
		$(PA_MOD_DIR)/module-example-sink.so

uninstall:
	rm -f $(PA_MOD_DIR)/module-example-source.so
	rm -f $(PA_MOD_DIR)/module-example-source-output.so
	rm -f $(PA_MOD_DIR)/module-example-sink-input.so
	rm -f $(PA_MOD_DIR)/module-example-sink.so

clean:
	rm -f $(CLIENTS) $(MODULES)

pa_play_simple: pa_play_simple.c
	$(CC) $(CFLAGS) -o $@ $< -lpulse-simple -lpulse

pa_play_async_cb: pa_play_async_cb.c
	$(CC) $(CFLAGS) -o $@ $< -lpulse

pa_play_async_poll: pa_play_async_poll.c
	$(CC) $(CFLAGS) -o $@ $< -lpulse

pa_record_simple: pa_record_simple.c
	$(CC) $(CFLAGS) -o $@ $< -lpulse-simple -lpulse

module-example-source.so: pa_module_source.c
	$(CC) $(CFLAGS) $(PA_MOD_FLAGS) -o $@ $<

module-example-source-output.so: pa_module_source_output.c
	$(CC) $(CFLAGS) $(PA_MOD_FLAGS) -o $@ $<

module-example-sink-input.so: pa_module_sink_input.c
	$(CC) $(CFLAGS) $(PA_MOD_FLAGS) -o $@ $<

module-example-sink.so: pa_module_sink.c
	$(CC) $(CFLAGS) $(PA_MOD_FLAGS) -o $@ $<
