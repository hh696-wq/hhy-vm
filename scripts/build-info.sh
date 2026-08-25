#!/bin/sh
set -eu

binary=${1:?binary path is required}

echo "HHY_VERSION=$(cat VERSION)"
echo "SYSTEM=$(uname -s)"
echo "ARCH=$(uname -m)"
echo "COMPILER=$(${CC:-cc} --version | sed -n '1p')"
echo "LIBCURL=$(curl-config --version 2>/dev/null || echo unknown)"
echo "PCRE2=$(pcre2-config --version 2>/dev/null || echo unknown)"
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
