CC ?= cc
FUZZ_CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
DEPFLAGS := -MMD -MP
CPPFLAGS ?= -Iinclude
VERSION := $(shell cat VERSION)
CPPFLAGS += -DHHY_VERSION=\"$(VERSION)\"
PCRE2_PREFIX ?= $(shell brew --prefix pcre2 2>/dev/null)
GC_PREFIX ?= $(shell brew --prefix bdw-gc 2>/dev/null)
JANSSON_PREFIX ?= $(shell brew --prefix jansson 2>/dev/null)
OPENSSL_PREFIX ?= $(shell brew --prefix openssl@3 2>/dev/null)
ifneq ($(PCRE2_PREFIX),)
CPPFLAGS += -I$(PCRE2_PREFIX)/include
LDFLAGS += -L$(PCRE2_PREFIX)/lib
endif
ifneq ($(GC_PREFIX),)
CPPFLAGS += -I$(GC_PREFIX)/include
LDFLAGS += -L$(GC_PREFIX)/lib
endif
ifneq ($(JANSSON_PREFIX),)
CPPFLAGS += -I$(JANSSON_PREFIX)/include
LDFLAGS += -L$(JANSSON_PREFIX)/lib
endif
ifneq ($(OPENSSL_PREFIX),)
CPPFLAGS += -I$(OPENSSL_PREFIX)/include
LDFLAGS += -L$(OPENSSL_PREFIX)/lib
endif
LDLIBS ?= -lcurl -lpcre2-8 -lgc -ljansson -lcrypto -lm

SOURCES := $(wildcard src/*.c)
SOURCE_NAMES := $(notdir $(SOURCES:.c=.o))
OBJECTS := $(addprefix build/release/,$(SOURCE_NAMES))
DEBUG_OBJECTS := $(addprefix build/debug/,$(SOURCE_NAMES))
TARGET := build/hhy
DEBUG_TARGET := build/hhy-debug
FUZZ_TARGET := build/hhy-fuzz
BYTECODE_TEST_TARGET := build/hhy-bytecode-test
LIBFUZZ_TARGET := build/hhy-libfuzzer
FUZZ_SOURCES := $(filter-out src/main.c,$(SOURCES)) tests/fuzz_runtime.c
SYSTEM := $(shell uname -s | tr '[:upper:]' '[:lower:]')
MACHINE := $(shell uname -m)
ARCH := $(if $(filter arm64 aarch64,$(MACHINE)),arm64,$(MACHINE))
PACKAGE := hhy-$(VERSION)-$(SYSTEM)-$(ARCH)
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean extensions test test-bytecode workload-test test-debug debug benchmark benchmark-bytecode benchmark-profiler benchmark-bytecode-cache quality install dist registry-package bytecode-test fuzz fuzz-smoke fuzz-libfuzzer fuzz-ci

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

build/release/%.o: src/%.c
	@mkdir -p build/release
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/release/main.o: VERSION

SANITIZERS ?= address,undefined
DEBUG_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 -g3 -fsanitize=$(SANITIZERS)

$(DEBUG_TARGET): $(DEBUG_OBJECTS)
	@mkdir -p build
	$(CC) $(DEBUG_CFLAGS) $(LDFLAGS) $(DEBUG_OBJECTS) $(LDLIBS) -o $@

build/debug/%.o: src/%.c
	@mkdir -p build/debug
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) $(DEPFLAGS) -c $< -o $@

build/debug/main.o: VERSION

debug: $(DEBUG_TARGET)

extensions:
	$(MAKE) -C extensions/sample
	$(MAKE) -C extensions/database
	$(MAKE) -C extensions/html

$(BYTECODE_TEST_TARGET): tests/bytecode_alpha.c src/bytecode.c src/common.c include/hhy/bytecode.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/bytecode_alpha.c src/bytecode.c src/common.c -o $@

bytecode-test: $(BYTECODE_TEST_TARGET)
	$(BYTECODE_TEST_TARGET)

test: $(TARGET) extensions bytecode-test
	sh tests/run.sh $(TARGET)

test-bytecode: $(TARGET) extensions bytecode-test
	HHY_ENGINE=bytecode sh tests/run.sh $(TARGET)

workload-test: $(TARGET) extensions
	python3 scripts/run-bytecode-workloads.py --binary $(TARGET)

test-debug: $(DEBUG_TARGET) extensions bytecode-test
	HHY_SKIP_GC_STRESS=1 sh tests/run.sh $(DEBUG_TARGET)

benchmark: $(TARGET)
	python3 scripts/run-benchmarks.py --binary $(TARGET)
	python3 scripts/check-performance.py

benchmark-bytecode: $(TARGET)
	python3 scripts/run-benchmarks.py --binary $(TARGET) --iterations 15 --output build/benchmarks/engine-comparison.json
	python3 scripts/check-performance.py --report build/benchmarks/engine-comparison.json
	python3 scripts/check-bytecode-rc.py --require-ready
	$(MAKE) benchmark-profiler
	$(MAKE) benchmark-bytecode-cache

benchmark-profiler: $(TARGET)
	python3 scripts/check-profiler-overhead.py --binary $(TARGET)

benchmark-bytecode-cache: $(TARGET)
	python3 scripts/evaluate-bytecode-cache.py --binary $(TARGET)
	python3 scripts/check-bytecode-cache-decision.py

quality: $(TARGET)
	python3 tests/check_contracts.py
	python3 tests/check_runtime_governance.py
	python3 tests/check-bytecode-cache-governance.py $(TARGET)
	sh tests/check-promotion-assets.sh
	sh tests/check-docs.sh $(TARGET) README.md
	npm run check --prefix editors
	$(MAKE) benchmark

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
	$(MAKE) all extensions
	rm -rf build/$(PACKAGE)
	mkdir -p build/$(PACKAGE)/bin build/$(PACKAGE)/lib build/$(PACKAGE)/examples \
		build/$(PACKAGE)/extensions/sample/bin build/$(PACKAGE)/extensions/database/bin \
		build/$(PACKAGE)/extensions/html/bin dist
	cp $(TARGET) build/$(PACKAGE)/bin/hhy
	cp README.md INSTALL.md LICENSE NOTICE build/$(PACKAGE)/
	CC="$(CC)" sh scripts/build-info.sh $(TARGET) > build/$(PACKAGE)/BUILD_INFO.txt
	cp examples/*.hhy examples/README.md build/$(PACKAGE)/examples/
	cp extensions/README.md build/$(PACKAGE)/extensions/
	cp extensions/sample/hhy.toml extensions/sample/bin/hhy-sample build/$(PACKAGE)/extensions/sample/
	mv build/$(PACKAGE)/extensions/sample/hhy-sample build/$(PACKAGE)/extensions/sample/bin/
	cp extensions/database/hhy.toml extensions/database/bin/hhy-database build/$(PACKAGE)/extensions/database/
	mv build/$(PACKAGE)/extensions/database/hhy-database build/$(PACKAGE)/extensions/database/bin/
	cp extensions/html/hhy.toml extensions/html/bin/hhy-html build/$(PACKAGE)/extensions/html/
	mv build/$(PACKAGE)/extensions/html/hhy-html build/$(PACKAGE)/extensions/html/bin/
	sh scripts/bundle-runtime.sh build/$(PACKAGE)
	COPYFILE_DISABLE=1 tar -C build -czf dist/$(PACKAGE).tar.gz $(PACKAGE)
	@if command -v sha256sum >/dev/null 2>&1; then \
		(cd dist && sha256sum $(PACKAGE).tar.gz > $(PACKAGE).tar.gz.sha256); \
	else \
		(cd dist && shasum -a 256 $(PACKAGE).tar.gz > $(PACKAGE).tar.gz.sha256); \
	fi

registry-package: extensions
	python3 scripts/build-registry-package.py

clean:
	rm -f src/*.o
	rm -rf build
	$(MAKE) -C extensions/sample clean
	$(MAKE) -C extensions/database clean
	$(MAKE) -C extensions/html clean

-include $(OBJECTS:.o=.d) $(DEBUG_OBJECTS:.o=.d)
