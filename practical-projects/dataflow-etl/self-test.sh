#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
hhy_bin=${HHY_BIN:-"$project_dir/../../build/hhy"}
output_dir="$project_dir/output"
mkdir -p "$output_dir"

python3 "$project_dir/test-server.py" &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT INT TERM
sleep 1

"$hhy_bin" check "$project_dir/etl.hhy"
"$hhy_bin" run "$project_dir/etl.hhy" \
  "$project_dir/fixtures/input" \
  "$project_dir/config/test.json" \
  "$output_dir/report.json" \
  "$output_dir/customers.csv"
python3 "$project_dir/test-report.py" "$output_dir/report.json" "$output_dir/customers.csv"
echo "DataFlow ETL self-test passed"
