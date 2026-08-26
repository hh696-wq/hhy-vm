CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
JANSSON_PREFIX ?= $(shell brew --prefix jansson 2>/dev/null)
CPPFLAGS ?= $(if $(JANSSON_PREFIX),-I$(JANSSON_PREFIX)/include,)
LDFLAGS ?= $(if $(JANSSON_PREFIX),-L$(JANSSON_PREFIX)/lib,) -ljansson
TARGET := bin/hhy-sample

.PHONY: all clean
all: $(TARGET)

$(TARGET): src/main.c
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -rf bin
