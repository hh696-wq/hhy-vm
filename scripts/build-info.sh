#!/bin/sh
set -eu

binary=${1:?binary path is required}

echo "HHY_VERSION=$(cat VERSION)"
echo "SYSTEM=$(uname -s)"
echo "ARCH=$(uname -m)"
echo "COMPILER=$(${CC:-cc} --version | sed -n '1p')"
echo "LIBCURL=$(curl-config --version 2>/dev/null || echo unknown)"
echo "PCRE2=$(pcre2-config --version 2>/dev/null || echo unknown)"
echo "DATABASE_VERSION=$(sed -n 's/^version = "\([^"]*\)"/\1/p' extensions/database/hhy.toml)"
echo "POSTGRESQL_CLIENT=$(pg_config --version 2>/dev/null || echo unavailable)"
echo "MYSQL_CLIENT=$(mysql_config --version 2>/dev/null || echo unavailable)"
if command -v brew >/dev/null 2>&1; then
    echo "BDWGC=$(brew list --versions bdw-gc 2>/dev/null || echo unknown)"
elif command -v dpkg-query >/dev/null 2>&1; then
    echo "BDWGC=$(dpkg-query -W -f='${Version}' libgc-dev 2>/dev/null || echo unknown)"
else
    echo "BDWGC=unknown"
fi
echo "LINKED_LIBRARIES:"
if command -v otool >/dev/null 2>&1; then
    otool -L "$binary"
elif command -v ldd >/dev/null 2>&1; then
    ldd "$binary"
else
    echo "link inspection unavailable"
fi
