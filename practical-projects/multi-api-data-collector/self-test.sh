#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
hhy_bin=${HHY_BIN:-"$project_dir/../../build/hhy"}
workspace=$(mktemp -d "${TMPDIR:-/tmp}/hhy-multi-api.XXXXXX")
server_pid=""

cleanup() {
    if [ -n "$server_pid" ]; then
        kill "$server_pid" 2>/dev/null || true
    fi
    rm -rf "$workspace"
}
trap cleanup EXIT INT TERM

python3 "$project_dir/test-server.py" "$workspace/port" &
server_pid=$!
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -s "$workspace/port" ] && break
    sleep 0.1
done
port=$(cat "$workspace/port")

cat > "$workspace/config.json" <<EOF
{
  "project": "Multi-API Fixture",
  "topic": "artificial intelligence",
  "pages": 2,
  "per_page": 2,
  "parallelism": 3,
  "user_agent": "HHY-Multi-API-Collector-Self-Test/1.0",
  "openalex_url": "http://127.0.0.1:$port/openalex",
  "crossref_url": "http://127.0.0.1:$port/crossref",
  "github_url": "http://127.0.0.1:$port/github"
}
EOF

"$hhy_bin" check "$project_dir/collector.hhy"
"$hhy_bin" run "$project_dir/collector.hhy" "$workspace/config.json" "$workspace/records.csv" "$workspace/report.json" "$workspace/failures.json"
"$hhy_bin" run "$project_dir/collector.hhy" "$workspace/config.json" "$workspace/records.csv" "$workspace/report.json" "$workspace/failures.json"
python3 "$project_dir/test-report.py" "$workspace/report.json" "$workspace/failures.json" "$workspace/records.csv"
echo "HHY Multi-API Data Collector self-test passed"
