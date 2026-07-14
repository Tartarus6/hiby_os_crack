# Makefile for open_hiby_player

# Target executables
HOST_BIN = open_hiby_player_host
TARGET_BIN = open_hiby_player_target

# Compiler and Linker configuration
# TODO: remove host config, switch to only target
CC = gcc
# CROSS_CC = mipsel-rockbox-linux-gnu-gcc
CROSS_CC ?= mipsel-linux-musl-gcc

# TODO: might need to have different sysroot for different targets. could extract it from a base firmware
SYSROOT ?= $(CURDIR)/sysroot

# LVGL directory name
LVGL_DIR = lvgl

# Self-bootstrap: clone LVGL if it doesn't exist yet before evaluating variables
ifeq ($(wildcard $(LVGL_DIR)),)
$(info Cloning LVGL v9.1.0...)
$(shell git clone --depth 1 -b v9.1.0 https://github.com/lvgl/lvgl.git)
endif

# Compile flags
# -DLV_CONF_INCLUDE_SIMPLE=1 is required to include lv_conf.h as "lv_conf.h"
CFLAGS = -O3 -g -Wall -I. -I$(LVGL_DIR) -DLV_CONF_INCLUDE_SIMPLE=1
HOST_CFLAGS = $(CFLAGS) -DHOST_BUILD=1 $(shell sdl2-config --cflags)
TARGET_CFLAGS = $(CFLAGS) -I$(LVGL_DIR)/src

# Link flags
HOST_LDFLAGS = $(shell sdl2-config --libs) -lpthread -lm -ldl
SYSROOT_USR_LIB = $(SYSROOT)/usr/lib
SYSROOT_LIB     = $(SYSROOT)/lib
TARGET_LDFLAGS = \
	-L$(SYSROOT_USR_LIB) \
	-L$(SYSROOT_LIB) \
	-Wl,--rpath-link,$(SYSROOT_USR_LIB) \
	-Wl,--rpath-link,$(SYSROOT_LIB) \
	-lsndfile -lasound -lpthread -lm -ldl -latomic

# Source files
APP_SRCS := $(shell find src -name '*.c')
LVGL_SRCS = $(shell find $(LVGL_DIR)/src -type f -name '*.c')

# Object files
HOST_OBJS = $(APP_SRCS:src/%.c=build_host/%.o) $(LVGL_SRCS:$(LVGL_DIR)/%.c=build_host/lvgl/%.o)
TARGET_OBJS = $(APP_SRCS:src/%.c=build_target/%.o) $(LVGL_SRCS:$(LVGL_DIR)/%.c=build_target/lvgl/%.o)

.PHONY: all host target clean compile_commands.json

# Default target builds for host simulation and generates compile commands for IDE
all: host compile_commands.json

# Build for host (Linux PC)
host: $(HOST_BIN)

$(HOST_BIN): $(HOST_OBJS)
	$(CC) -o $@ $(HOST_OBJS) $(HOST_LDFLAGS)
	@echo "Host build complete: Run './$(HOST_BIN)' to start the simulator."

build_host/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) -c $< -o $@

build_host/lvgl/%.o: $(LVGL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) -c $< -o $@

# Build for target (MIPS HiBy Device)
target: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_OBJS)
	$(CROSS_CC) -o $@ $(TARGET_OBJS) $(TARGET_LDFLAGS)
	@echo "Target build complete: File ready at '$(TARGET_BIN)'"

build_target/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CROSS_CC) $(TARGET_CFLAGS) -c $< -o $@

build_target/lvgl/%.o: $(LVGL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CROSS_CC) $(TARGET_CFLAGS) -c $< -o $@

# Generate compile_commands.json for Zed/clangd LSP autofill and hover popups
compile_commands.json:
	@python3 generate_compile_commands.py

clean:
	rm -rf build_host build_target $(HOST_BIN) $(TARGET_BIN) compile_commands.json compile_flags.txt
