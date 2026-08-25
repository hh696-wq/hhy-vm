CC ?= cc
FUZZ_CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude
VERSION := $(shell cat VERSION)
CPPFLAGS += -DHHY_VERSION=\"$(VERSION)\"
PCRE2_PREFIX ?= $(shell brew --prefix pcre2 2>/dev/null)
GC_PREFIX ?= $(shell brew --prefix bdw-gc 2>/dev/null)
ifneq ($(PCRE2_PREFIX),)
CPPFLAGS += -I$(PCRE2_PREFIX)/include
LDFLAGS += -L$(PCRE2_PREFIX)/lib
endif
ifneq ($(GC_PREFIX),)
CPPFLAGS += -I$(GC_PREFIX)/include
LDFLAGS += -L$(GC_PREFIX)/lib
endif
LDLIBS ?= -lcurl -lpcre2-8 -lgc -lm

SOURCES := $(wildcard src/*.c)
SOURCE_NAMES := $(notdir $(SOURCES:.c=.o))
OBJECTS := $(addprefix build/release/,$(SOURCE_NAMES))
DEBUG_OBJECTS := $(addprefix build/debug/,$(SOURCE_NAMES))
TARGET := build/hhy
DEBUG_TARGET := build/hhy-debug
FUZZ_TARGET := build/hhy-fuzz
LIBFUZZ_TARGET := build/hhy-libfuzzer
FUZZ_SOURCES := $(filter-out src/main.c,$(SOURCES)) tests/fuzz_runtime.c
SYSTEM := $(shell uname -s | tr '[:upper:]' '[:lower:]')
MACHINE := $(shell uname -m)
ARCH := $(if $(filter arm64 aarch64,$(MACHINE)),arm64,$(MACHINE))
PACKAGE := hhy-$(VERSION)-$(SYSTEM)-$(ARCH)
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean test test-debug debug install dist fuzz fuzz-smoke fuzz-libfuzzer fuzz-ci

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

build/release/%.o: src/%.c
	@mkdir -p build/release
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

DEBUG_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 -g3 -fsanitize=address,undefined

$(DEBUG_TARGET): $(DEBUG_OBJECTS)
	@mkdir -p build
	$(CC) $(DEBUG_CFLAGS) $(LDFLAGS) $(DEBUG_OBJECTS) $(LDLIBS) -o $@

build/debug/%.o: src/%.c
	@mkdir -p build/debug
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -c $< -o $@

debug: $(DEBUG_TARGET)

test: $(TARGET)
	sh tests/run.sh $(TARGET)

test-debug: $(DEBUG_TARGET)
	sh tests/run.sh $(DEBUG_TARGET)

fuzz:
	@mkdir -p build
	$(CC) $(CPPFLAGS) -DHHY_STANDALONE_FUZZ -std=c11 -Wall -Wextra -Wpedantic -O1 -g \
		-fsanitize=address,undefined $(FUZZ_SOURCES) $(LDFLAGS) $(LDLIBS) -o $(FUZZ_TARGET)

fuzz-smoke: fuzz
	$(FUZZ_TARGET) 1000

fuzz-libfuzzer:
	@mkdir -p build
	$(FUZZ_CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -O1 -g \
		-fsanitize=fuzzer,address,undefined $(FUZZ_SOURCES) $(LDFLAGS) $(LDLIBS) -o $(LIBFUZZ_TARGET)

fuzz-ci: fuzz-libfuzzer
	rm -rf build/fuzz-corpus
	cp -R tests/fuzz-corpus build/fuzz-corpus
	$(LIBFUZZ_TARGET) -max_total_time=15 -timeout=5 -rss_limit_mb=1024 build/fuzz-corpus

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/hhy

dist:
	$(MAKE) clean
	$(MAKE) all
	rm -rf build/$(PACKAGE)
	mkdir -p build/$(PACKAGE)/bin build/$(PACKAGE)/docs build/$(PACKAGE)/examples dist
	cp $(TARGET) build/$(PACKAGE)/bin/hhy
	cp README.md INSTALL.md build/$(PACKAGE)/
	cp docs/HHY_V1.md docs/DEPENDENCIES.md docs/EXTENSION_ROADMAP.md docs/THIRD_PARTY_NOTICES.md docs/KNOWN_LIMITATIONS.md build/$(PACKAGE)/docs/
	CC="$(CC)" sh scripts/build-info.sh $(TARGET) > build/$(PACKAGE)/BUILD_INFO.txt
	cp examples/*.hhy examples/README.md build/$(PACKAGE)/examples/
	COPYFILE_DISABLE=1 tar -C build -czf dist/$(PACKAGE).tar.gz $(PACKAGE)
	@if command -v sha256sum >/dev/null 2>&1; then \
		sha256sum dist/$(PACKAGE).tar.gz > dist/$(PACKAGE).tar.gz.sha256; \
	else \
		shasum -a 256 dist/$(PACKAGE).tar.gz > dist/$(PACKAGE).tar.gz.sha256; \
	fi

clean:
	rm -f src/*.o
	rm -rf build
