#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
hhy_bin=${HHY_BIN:-"$project_dir/../../build/hhy"}
output_dir="$project_dir/output"
workspace=$(mktemp -d "${TMPDIR:-/tmp}/hhy-wikipedia.XXXXXX")
server_pid=""

cleanup() {
    if [ -n "$server_pid" ]; then
        kill "$server_pid" 2>/dev/null || true
    fi
    rm -rf "$workspace"
}
trap cleanup EXIT INT TERM

mkdir -p "$output_dir"
python3 "$project_dir/test-server.py" "$workspace/port" &
server_pid=$!

for _ in 1 2 3 4 5 6 7 8 9 10; do
    if [ -s "$workspace/port" ]; then
        break
    fi
    sleep 0.1
done

port=$(cat "$workspace/port")
cat > "$workspace/config.json" <<EOF
{
  "project": "Hong Kong Film Companies Fixture",
  "api_base": "http://127.0.0.1:$port/w/api.php",
  "search_url": "http://127.0.0.1:$port/w/api.php?action=query&list=search&srsearch=fixture&format=json&formatversion=2",
  "keyword": "香港電影公司",
  "parallelism": 3,
  "user_agent": "HHY-Wikipedia-Research-Self-Test/1.0"
}
EOF

"$hhy_bin" check "$project_dir/crawl.hhy"
"$hhy_bin" run "$project_dir/crawl.hhy" \
    "$workspace/config.json" \
    "$output_dir/report.json" \
    "$output_dir/hong-kong-film-companies.csv"
python3 "$project_dir/test-report.py" \
    "$output_dir/report.json" \
    "$output_dir/hong-kong-film-companies.csv"

echo "HHY Wikipedia Research self-test passed"
