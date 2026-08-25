#!/bin/sh
set -eu

stage=${1:?usage: bundle-runtime.sh STAGE_DIRECTORY}
binary="$stage/bin/hhy"
libdir="$stage/lib"

mkdir -p "$libdir"

case "$(uname -s)" in
  Darwin)
    otool -L "$binary" | awk 'NR > 1 { print $1 }' | while IFS= read -r dependency; do
      case "$dependency" in
        /opt/homebrew/*|/usr/local/*)
          name=$(basename "$dependency")
          cp -L "$dependency" "$libdir/$name"
          chmod u+w "$libdir/$name"
          install_name_tool -change "$dependency" "@executable_path/../lib/$name" "$binary"
          install_name_tool -id "@loader_path/$name" "$libdir/$name"
          ;;
      esac
    done
    for library in "$libdir"/*; do
      [ -f "$library" ] || continue
      codesign --force --sign - "$library" >/dev/null
    done
    codesign --force --sign - "$binary" >/dev/null
    ;;
  Linux)
    command -v patchelf >/dev/null 2>&1 || {
      echo "patchelf is required to build a relocatable Linux release" >&2
      exit 1
    }
    ldd "$binary" | awk '/=> \/|^\// { for (i = 1; i <= NF; i++) if ($i ~ /^\//) print $i }' | while IFS= read -r dependency; do
      name=$(basename "$dependency")
      case "$name" in
        ld-linux-*|ld-musl-*|libc.so.*|libm.so.*|libpthread.so.*|librt.so.*|libdl.so.*) continue ;;
      esac
      cp -L "$dependency" "$libdir/$name"
      chmod u+w "$libdir/$name"
    done
    patchelf --set-rpath '$ORIGIN/../lib' "$binary"
    for library in "$libdir"/*; do
      [ -f "$library" ] || continue
      patchelf --set-rpath '$ORIGIN' "$library"
    done
    ;;
  *)
    echo "unsupported release platform: $(uname -s)" >&2
    exit 1
    ;;
esac

"$binary" --version
