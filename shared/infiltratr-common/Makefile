# SPDX-License-Identifier: GPL-3.0-or-later
CC ?= cc
AR ?= ar
BUILD_DIR ?= ../../build/infiltratr-common-standalone
CPPFLAGS += -Iinclude
CFLAGS ?= -O2 -g
override CFLAGS += -std=c11 -fPIC -Wall -Wextra -Wpedantic -Werror \
	-Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes
OBJECTS := $(BUILD_DIR)/core.o $(BUILD_DIR)/posix.o
ARCHIVE := $(BUILD_DIR)/libinfiltratr-common.a
SHARED := $(BUILD_DIR)/libinfiltratr-common.so.1.1.1

.PHONY: all check clean shared

all: $(ARCHIVE)

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/core.o: src/core.c include/infiltratr/core.h \
	include/infiltratr/compiler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/posix.o: src/posix.c include/infiltratr/core.h \
	include/infiltratr/posix.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(ARCHIVE): $(OBJECTS)
	$(AR) rcsD $@ $(OBJECTS)

shared: $(SHARED)

$(SHARED): $(OBJECTS)
	$(CC) -shared -Wl,-soname,libinfiltratr-common.so.1 \
		-o $@ $(OBJECTS) -lm

$(BUILD_DIR)/core-smoke: tests/core_smoke.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@

check: $(BUILD_DIR)/core-smoke
	./$(BUILD_DIR)/core-smoke

clean:
	rm -rf "$(BUILD_DIR)"
