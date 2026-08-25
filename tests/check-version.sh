#!/bin/sh
set -eu

HHY_BIN=${1:-build/hhy}
VERSION=$(cat VERSION)

for document in \
    README.md \
    INSTALL.md \
    docs/HHY_V1.md \
    docs/KNOWN_LIMITATIONS.md \
    docs/DEPENDENCIES.md \
    docs/EXTENSION_ROADMAP.md \
    examples/README.md
do
    grep -F "\`$VERSION\`" "$document" >/dev/null || {
        echo "version mismatch: $document does not declare current version $VERSION" >&2
        exit 1
    }
done

actual=$($HHY_BIN --version)
expected="hhy $VERSION"
if [ "$actual" != "$expected" ]; then
    echo "version mismatch: executable reports '$actual', expected '$expected'" >&2
    exit 1
fi

echo "validated project version $VERSION"
