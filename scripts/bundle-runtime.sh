#!/bin/sh
set -eu

stage=${1:?usage: bundle-runtime.sh STAGE_DIRECTORY}
binary="$stage/bin/hhy"
libdir="$stage/lib"

mkdir -p "$libdir"

case "$(uname -s)" in
  Darwin)
    python3 scripts/bundle-darwin.py "$stage"
    ;;
  Linux)
    command -v patchelf >/dev/null 2>&1 || {
      echo "patchelf is required to build a relocatable Linux release" >&2
      exit 1
    }
    for executable in "$binary" "$stage"/extensions/*/bin/*; do
      [ -f "$executable" ] || continue
      ldd "$executable" | awk '/=> \/|^\// { for (i = 1; i <= NF; i++) if ($i ~ /^\//) print $i }' | while IFS= read -r dependency; do
        name=$(basename "$dependency")
        case "$name" in
          ld-linux-*|ld-musl-*|libc.so.*|libm.so.*|libpthread.so.*|librt.so.*|libdl.so.*) continue ;;
        esac
        cp -L "$dependency" "$libdir/$name"
        chmod u+w "$libdir/$name"
      done
      case "$executable" in
        "$binary") patchelf --set-rpath '$ORIGIN/../lib' "$executable" ;;
        *) patchelf --set-rpath '$ORIGIN/../lib' "$executable" ;;
      esac
    done
    for library in "$libdir"/*; do
      [ -f "$library" ] || continue
      patchelf --set-rpath '$ORIGIN' "$library"
    done
    for extension in "$stage"/extensions/*; do
      [ -d "$extension/bin" ] || continue
      mkdir -p "$extension/lib"
      cp "$libdir"/* "$extension/lib/"
    done
    ;;
  *)
    echo "unsupported release platform: $(uname -s)" >&2
    exit 1
    ;;
esac

"$binary" --version
