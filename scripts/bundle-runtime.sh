#!/bin/sh
set -eu

stage=${1:?usage: bundle-runtime.sh STAGE_DIRECTORY}
binary="$stage/bin/hhy"
libdir="$stage/lib"

mkdir -p "$libdir"

case "$(uname -s)" in
  Darwin)
    for executable in "$binary" "$stage"/extensions/*/bin/*; do
      [ -f "$executable" ] || continue
      case "$executable" in
        "$binary") relative_lib="../lib" ;;
        *) relative_lib="../lib" ;;
      esac
      otool -L "$executable" | awk 'NR > 1 { print $1 }' | while IFS= read -r dependency; do
        case "$dependency" in
          /opt/homebrew/*|/usr/local/*)
            name=$(basename "$dependency")
            cp -L "$dependency" "$libdir/$name"
            chmod u+w "$libdir/$name"
            install_name_tool -change "$dependency" "@executable_path/$relative_lib/$name" "$executable"
            install_name_tool -id "@loader_path/$name" "$libdir/$name"
            ;;
        esac
      done
    done
    for library in "$libdir"/*; do
      [ -f "$library" ] || continue
      otool -L "$library" | awk 'NR > 1 { print $1 }' | while IFS= read -r dependency; do
        case "$dependency" in
          /opt/homebrew/*|/usr/local/*)
            name=$(basename "$dependency")
            [ -f "$libdir/$name" ] && install_name_tool -change "$dependency" "@loader_path/$name" "$library"
            ;;
        esac
      done
      codesign --force --sign - "$library" >/dev/null
    done
    for executable in "$binary" "$stage"/extensions/*/bin/*; do
      [ -f "$executable" ] && codesign --force --sign - "$executable" >/dev/null
    done
    for extension in "$stage"/extensions/*; do
      [ -d "$extension/bin" ] || continue
      mkdir -p "$extension/lib"
      cp "$libdir"/* "$extension/lib/"
    done
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
