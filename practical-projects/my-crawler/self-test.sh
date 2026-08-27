#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$project_dir/../.." && pwd)
hhy_bin=${HHY_BIN:-"$repo_root/build/hhy"}
workspace=$(mktemp -d "${TMPDIR:-/tmp}/hhy-crawler.XXXXXX")
server_pid=""

cleanup() {
    if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || true; fi
    rm -rf "$workspace"
}
trap cleanup EXIT INT TERM

HHY_BIN="$hhy_bin" "$project_dir/init.sh"
python3 "$project_dir/test-server.py" "$workspace/port" &
server_pid=$!
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -s "$workspace/port" ] && break
    sleep 0.1
done
port=$(cat "$workspace/port")
cat > "$workspace/config.json" <<EOF
{
  "project": "Crawler Fixture",
  "seeds": ["http://127.0.0.1:$port/page"],
  "parallelism": 2,
  "user_agent": "HHY-Collector-Self-Test/1.0",
  "root_selector": "section.card",
  "max_results": 10,
  "schema": {
    "title": { "selector": "h2", "value": "text", "required": true },
    "anchor": { "selector": "h2", "value": "attr", "name": "id" }
  }
}
EOF

HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" check "$project_dir/crawler.hhy"
HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" run "$project_dir/crawler.hhy" \
    "$workspace/config.json" "$workspace/records.json" "$workspace/report.json" "$workspace/failures.json"
python3 "$project_dir/test-report.py" "$workspace/records.json" "$workspace/report.json" "$workspace/failures.json"
echo "HHY Collector Framework self-test passed"
