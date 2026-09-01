#!/bin/sh
set -eu

version=$(cat VERSION)
package="hhy-${version}-windows-x86_64"
stage="build/${package}"
archive="dist/${package}.tar.gz"

runtime=build/hhy.exe
[ -f "$runtime" ] || runtime=build/hhy
sample=extensions/sample/bin/hhy-sample.exe
[ -f "$sample" ] || sample=extensions/sample/bin/hhy-sample
html=extensions/html/bin/hhy-html.exe
[ -f "$html" ] || html=extensions/html/bin/hhy-html

for executable in "$runtime" "$sample" "$html"; do
    [ -f "$executable" ] || { echo "missing Windows executable: $executable" >&2; exit 1; }
done

rm -rf "$stage"
mkdir -p "$stage/bin" "$stage/examples" \
    "$stage/extensions/sample/bin" "$stage/extensions/sample/lib" \
    "$stage/extensions/html/bin" "$stage/extensions/html/lib" dist
cp "$runtime" "$stage/bin/hhy.exe"
cp README.md INSTALL.md LICENSE NOTICE "$stage/"
CC="${CC:-cc}" sh scripts/build-info.sh "$runtime" > "$stage/BUILD_INFO.txt"
cp examples/*.hhy examples/README.md "$stage/examples/"
cp extensions/README.md "$stage/extensions/"
cp extensions/sample/hhy.toml "$stage/extensions/sample/"
cp "$sample" "$stage/extensions/sample/bin/hhy-sample.exe"
cp extensions/html/hhy.toml "$stage/extensions/html/"
cp "$html" "$stage/extensions/html/bin/hhy-html.exe"

ldd "$stage/bin/hhy.exe" | awk '/=> \/|^\// { for (i = 1; i <= NF; i++) if ($i ~ /^\//) print $i }' | \
    while IFS= read -r dependency; do
        cp -L "$dependency" "$stage/bin/$(basename "$dependency")"
    done
for executable in "$stage"/extensions/*/bin/*.exe; do
    extension_dir=$(dirname "$(dirname "$executable")")
    ldd "$executable" | awk '/=> \/|^\// { for (i = 1; i <= NF; i++) if ($i ~ /^\//) print $i }' | \
        while IFS= read -r dependency; do
            cp -L "$dependency" "$extension_dir/lib/$(basename "$dependency")"
        done
done

"$stage/bin/hhy.exe" --version
"$stage/bin/hhy.exe" bytecode "$stage/examples/00-hello.hhy" | grep 'HALT'
COPYFILE_DISABLE=1 tar -C build -czf "$archive" "$package"
(cd dist && sha256sum "${package}.tar.gz" > "${package}.tar.gz.sha256")
printf 'Windows release archive: %s\n' "$archive"
