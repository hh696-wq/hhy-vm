#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
hhy_bin=${HHY_BIN:-"$project_dir/../build/hhy"}
output_dir="$project_dir/output"

mkdir -p "$output_dir"
python3 "$project_dir/test-server.py" &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT INT TERM
sleep 1

"$hhy_bin" check "$project_dir/flowguard.hhy"
"$hhy_bin" run "$project_dir/flowguard.hhy" \
  "$project_dir/fixtures/healthy-project" \
  "$project_dir/config/healthy.json" \
  "$output_dir/healthy-report.json"
python3 "$project_dir/test-report.py" "$output_dir/healthy-report.json" true 0

if "$hhy_bin" run "$project_dir/flowguard.hhy" \
  "$project_dir/fixtures/risky-project" \
  "$project_dir/config/risky.json" \
  "$output_dir/risky-report.json"; then
    echo "expected risky fixture to fail" >&2
    exit 1
fi
python3 "$project_dir/test-report.py" "$output_dir/risky-report.json" false 5

echo "FlowGuard self-test passed"
