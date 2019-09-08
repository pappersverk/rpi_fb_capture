# Makefile for building the port binary
#
# Makefile targets:
#
# all/install   build and install
# clean         clean build products and intermediates
#
# Variables to override:
#
# MIX_APP_PATH  path to the build directory
#
# CC            C compiler
# CROSSCOMPILE	crosscompiler prefix, if any
# CFLAGS	compiler flags for compiling all C files
# LDFLAGS	linker flags for linking all binaries

PREFIX = $(MIX_APP_PATH)/priv
BUILD  = $(MIX_APP_PATH)/obj

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
        BIN = $(PREFIX)/rpi_fb_capture
    endif
    endif
else
# Crosscompiled build
BIN = $(PREFIX)/rpi_fb_capture
endif

LDFLAGS += -lm -lbcm_host -lvchostif

CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
CFLAGS += $(TARGET_CFLAGS)

# Enable for debug messages
# CFLAGS += -DDEBUG

SRC = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h)
OBJ = $(SRC:src/%.c=$(BUILD)/%.o)

calling_from_make:
	mix compile

all: install

install: $(PREFIX) $(BUILD) $(BIN)

$(OBJ): $(HEADERS) Makefile

$(BUILD)/%.o: src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(ERL_LDFLAGS) $(LDFLAGS)

$(PREFIX) $(BUILD):
	mkdir -p $@

clean:
	$(RM) $(BIN) $(OBJ)

format:
	astyle \
	    --style=kr \
	    --indent=spaces=4 \
	    --align-pointer=name \
	    --align-reference=name \
	    --convert-tabs \
	    --attach-namespaces \
	    --max-code-length=100 \
	    --max-instatement-indent=120 \
	    --pad-header \
	    --pad-oper \
	    $(SRC)

.PHONY: all clean calling_from_make install format
