#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$project_dir/../.." && pwd)
hhy_bin=${HHY_BIN:-"$repo_root/build/hhy"}
config=${1:?"usage: run.sh <config.json> [output-dir]"}
output_dir=${2:-"$project_dir/output"}

HHY_BIN="$hhy_bin" "$project_dir/init.sh"
HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" run "$project_dir/sitegraph.hhy" "$config" "$output_dir/inventory.json" "$output_dir/graph.json" "$output_dir/report.json" "$output_dir/failures.json"
