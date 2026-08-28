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
  "allowed_domains": ["127.0.0.1"],
  "allowed_path_prefixes": ["/"],
  "max_depth": 2,
  "max_pages": 4,
  "max_frontier": 10,
  "max_links_per_page": 20,
  "allow_private_networks": true,
  "follow_selector": "a[href]",
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
cat > "$workspace/ssrf.hhy" <<EOF
http.get("http://127.0.0.1:$port/page", { allow_private_networks: false }) |> send |> print
EOF
if "$hhy_bin" run "$workspace/ssrf.hhy" >"$workspace/ssrf.out" 2>&1; then
    echo "SSRF guard unexpectedly allowed loopback" >&2
    exit 1
fi
grep "private, loopback, or link-local" "$workspace/ssrf.out" >/dev/null
echo "HHY Collector Framework self-test passed"
