CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
JANSSON_PREFIX ?= $(shell brew --prefix jansson 2>/dev/null)
LEXBOR_PREFIX ?= $(shell brew --prefix lexbor 2>/dev/null)
CPPFLAGS ?= $(if $(JANSSON_PREFIX),-I$(JANSSON_PREFIX)/include,) \
	$(if $(LEXBOR_PREFIX),-I$(LEXBOR_PREFIX)/include,)
LDFLAGS ?= $(if $(JANSSON_PREFIX),-L$(JANSSON_PREFIX)/lib,) \
	$(if $(LEXBOR_PREFIX),-L$(LEXBOR_PREFIX)/lib,) -ljansson -llexbor
TARGET := bin/hhy-html

.PHONY: all clean test
all: $(TARGET)

$(TARGET): src/main.c
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

test: $(TARGET)
	sh tests/protocol.sh $(TARGET)

clean:
	rm -rf bin
