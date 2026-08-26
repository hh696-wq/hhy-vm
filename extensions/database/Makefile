CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
JANSSON_PREFIX ?= $(shell brew --prefix jansson 2>/dev/null)
PG_CONFIG ?= $(shell command -v pg_config 2>/dev/null || echo /opt/homebrew/opt/libpq/bin/pg_config)
ZSTD_PREFIX ?= $(shell brew --prefix zstd 2>/dev/null)
OPENSSL_PREFIX ?= $(shell brew --prefix openssl@3 2>/dev/null)
CPPFLAGS ?= $(if $(JANSSON_PREFIX),-I$(JANSSON_PREFIX)/include,) \
	-I$(shell $(PG_CONFIG) --includedir) $(shell mysql_config --cflags)
LDFLAGS ?= $(if $(JANSSON_PREFIX),-L$(JANSSON_PREFIX)/lib,) \
	-L$(shell $(PG_CONFIG) --libdir) $(if $(ZSTD_PREFIX),-L$(ZSTD_PREFIX)/lib,) \
	$(if $(OPENSSL_PREFIX),-L$(OPENSSL_PREFIX)/lib,) -lpq $(shell mysql_config --libs) -ljansson
SOURCES := $(wildcard src/*.c)
TARGET := bin/hhy-database

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

clean:
	rm -rf bin
