#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$project_dir/../.." && pwd)
hhy_bin=${HHY_BIN:-"$repo_root/build/hhy"}
extension_home=${HHY_EXTENSION_HOME:-"$project_dir/.hhy-extensions"}

if [ ! -x "$hhy_bin" ]; then
    echo "HHY binary not found: $hhy_bin" >&2
    echo "Run make from $repo_root first." >&2
    exit 1
fi

make -C "$repo_root/extensions/html"
mkdir -p "$extension_home" "$project_dir/output"
if ! HHY_EXTENSION_HOME="$extension_home" "$hhy_bin" list | grep -q '^html '; then
    HHY_EXTENSION_HOME="$extension_home" "$hhy_bin" install --yes "$repo_root/extensions/html"
fi

echo "my-crawler initialized"
echo "Extension home: $extension_home"
echo "Output: $project_dir/output"
