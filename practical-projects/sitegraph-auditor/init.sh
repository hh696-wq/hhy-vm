#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$project_dir/../.." && pwd)
hhy_bin=${HHY_BIN:-"$repo_root/build/hhy"}

make -C "$repo_root/extensions/html"
mkdir -p "$project_dir/.hhy-extensions" "$project_dir/output"
if ! HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" list | grep -q '^html '; then
    HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" install --yes "$repo_root/extensions/html" >/dev/null
fi
echo "SiteGraph Auditor initialized"
