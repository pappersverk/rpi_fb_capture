# Variables to override
#
# CC            C compiler
# CROSSCOMPILE	crosscompiler prefix, if any
# CFLAGS	compiler flags for compiling all C files
# LDFLAGS	linker flags for linking all binaries

TARGET_CFLAGS = $(shell src/detect_target.sh)

# Check that we're on a supported build platform
ifeq ($(CROSSCOMPILE),)
    # Not crosscompiling.
    ifeq ($(shell uname -s),Darwin)
        $(warning rpi_fb_capture only works on Nerves and Raspbian.)
        $(warning Skipping C compilation.)
    else
    ifeq ($(TARGET_CFLAGS),)
        $(warning rpi_fb_capture only works on Nerves and Raspbian.)
        $(warning If on Raspbian, I haven't tried this yet and there's probably an include path that's needed.)
        $(warning Skipping C compilation.)
    else
        DEFAULT_TARGETS = priv/rpi_fb_capture
    endif
    endif
else
# Crosscompiled build
DEFAULT_TARGETS = priv/rpi_fb_capture
endif

LDFLAGS += -lm -lbcm_host -lvchostif

CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
CFLAGS += $(TARGET_CFLAGS)

# Enable for debug messages
# CFLAGS += -DDEBUG

SRCS = src/main.c

.PHONY: all clean

all: priv $(DEFAULT_TARGETS)

priv:
	mkdir -p priv

priv/rpi_fb_capture: priv $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	$(RM) -rf priv/rpi_fb_capture src/*.o

format:
	astyle -n $(SRCS)

.PHONY: all clean format
