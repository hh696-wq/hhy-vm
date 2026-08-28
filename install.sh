#!/bin/sh
set -eu

REPOSITORY="${HHY_REPOSITORY:-hh696-wq/hhy-vm}"
INSTALL_ROOT="${HHY_INSTALL_ROOT:-${HOME}/.local/share/hhy}"
BIN_DIR="${HHY_BIN_DIR:-${HOME}/.local/bin}"

say() { printf '%s\n' "hhy-install: $*"; }
fail() { say "error: $*" >&2; exit 1; }

command -v tar >/dev/null 2>&1 || fail "tar is required"

VERSION="${HHY_VERSION:-}"
if [ -z "$VERSION" ]; then
  latest_api="https://api.github.com/repos/${REPOSITORY}/releases/latest"
  if command -v curl >/dev/null 2>&1; then
    latest_json="$(curl -fsSL --retry 3 --connect-timeout 15 "$latest_api")"
  elif command -v wget >/dev/null 2>&1; then
    latest_json="$(wget -q --tries=3 -O - "$latest_api")"
  else
    fail "curl or wget is required"
  fi
  VERSION="$(printf '%s\n' "$latest_json" | sed -n 's/.*"tag_name":[[:space:]]*"v\([^"]*\)".*/\1/p' | head -n 1)"
  [ -n "$VERSION" ] || fail "could not resolve the latest stable release"
fi
RELEASE_BASE_URL="${HHY_RELEASE_BASE_URL:-https://github.com/${REPOSITORY}/releases/download/v${VERSION}}"

case "$(uname -s)" in
  Darwin) platform="darwin" ;;
  Linux) platform="linux" ;;
  *) fail "unsupported operating system: $(uname -s)" ;;
esac

case "$(uname -m)" in
  arm64|aarch64) arch="arm64" ;;
  x86_64|amd64) arch="x86_64" ;;
  *) fail "unsupported CPU architecture: $(uname -m)" ;;
esac

[ "$platform-$arch" != "darwin-x86_64" ] || fail "macOS x86_64 is not an official build"

archive="hhy-${VERSION}-${platform}-${arch}.tar.gz"
package="hhy-${VERSION}-${platform}-${arch}"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/hhy-install.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

download() {
  url="$1"
  destination="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --connect-timeout 15 "$url" -o "$destination"
  elif command -v wget >/dev/null 2>&1; then
    wget -q --tries=3 -O "$destination" "$url"
  else
    fail "curl or wget is required"
  fi
}

say "downloading HHY ${VERSION} for ${platform}-${arch}"
download "${RELEASE_BASE_URL}/${archive}" "${tmp_dir}/${archive}"
download "${RELEASE_BASE_URL}/${archive}.sha256" "${tmp_dir}/${archive}.sha256"

expected="$(awk 'NR == 1 { print $1 }' "${tmp_dir}/${archive}.sha256")"
[ -n "$expected" ] || fail "the checksum file is empty"
if command -v sha256sum >/dev/null 2>&1; then
  actual="$(sha256sum "${tmp_dir}/${archive}" | awk '{ print $1 }')"
elif command -v shasum >/dev/null 2>&1; then
  actual="$(shasum -a 256 "${tmp_dir}/${archive}" | awk '{ print $1 }')"
else
  fail "sha256sum or shasum is required to verify the release"
fi
[ "$actual" = "$expected" ] || fail "SHA-256 verification failed"
say "verified SHA-256 ${actual}"

tar -xzf "${tmp_dir}/${archive}" -C "$tmp_dir"
[ -x "${tmp_dir}/${package}/bin/hhy" ] || fail "release does not contain ${package}/bin/hhy"

mkdir -p "$INSTALL_ROOT" "$BIN_DIR"
target="${INSTALL_ROOT}/${VERSION}"
staged="${INSTALL_ROOT}/.${VERSION}.install.$$"
rm -rf "$staged"
mv "${tmp_dir}/${package}" "$staged"
rm -rf "$target"
mv "$staged" "$target"
ln -sfn "${target}/bin/hhy" "${BIN_DIR}/hhy"

say "installed ${target}"
say "command: ${BIN_DIR}/hhy"
case ":${PATH}:" in
  *":${BIN_DIR}:"*) : ;;
  *) say "add this to your shell profile: export PATH=\"${BIN_DIR}:\$PATH\"" ;;
esac
"${BIN_DIR}/hhy" --version
