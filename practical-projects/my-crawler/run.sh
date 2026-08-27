#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$project_dir/../.." && pwd)
hhy_bin=${HHY_BIN:-"$repo_root/build/hhy"}
extension_home=${HHY_EXTENSION_HOME:-"$project_dir/.hhy-extensions"}
config=${1:-"$project_dir/config/hhylang.json"}
output_dir=${2:-"$project_dir/output"}

HHY_BIN="$hhy_bin" HHY_EXTENSION_HOME="$extension_home" "$project_dir/init.sh"
HHY_EXTENSION_HOME="$extension_home" "$hhy_bin" run "$project_dir/crawler.hhy" \
    "$config" "$output_dir/records.json" "$output_dir/report.json" "$output_dir/failures.json"
