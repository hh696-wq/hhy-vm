#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
hhy_bin=${HHY_BIN:-"$project_dir/../build/hhy"}
output_dir="$project_dir/output"
workspace=$(mktemp -d "${TMPDIR:-/tmp}/asset-governance.XXXXXX")
trap 'rm -rf "$workspace"' EXIT INT TERM
mkdir -p "$output_dir"
cp -R "$project_dir/fixtures/project/." "$workspace/"
touch -t 202001010000 "$workspace/tmp/old.tmp"

"$hhy_bin" check "$project_dir/audit.hhy"
"$hhy_bin" check "$project_dir/cleanup.hhy"

if "$hhy_bin" run "$project_dir/audit.hhy" "$workspace" "$project_dir/config/test.json" "$output_dir/report.json"; then
    echo "expected critical audit finding" >&2
    exit 1
fi
python3 "$project_dir/test-report.py" "$output_dir/report.json"

"$hhy_bin" run --dry-run "$project_dir/cleanup.hhy" "$workspace" "$output_dir/report.json"
python3 "$project_dir/test-cleanup.py" "$workspace" dry-run

"$hhy_bin" run "$project_dir/cleanup.hhy" "$workspace" "$output_dir/report.json"
python3 "$project_dir/test-cleanup.py" "$workspace" applied
echo "Asset Governance self-test passed"
