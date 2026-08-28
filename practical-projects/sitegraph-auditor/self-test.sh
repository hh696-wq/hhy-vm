#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$project_dir/../.." && pwd)
hhy_bin=${HHY_BIN:-"$repo_root/build/hhy"}
workspace=$(mktemp -d "${TMPDIR:-/tmp}/hhy-sitegraph.XXXXXX")
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

write_config() {
    name=$1
    seed=$2
    cat > "$workspace/$name.json" <<EOF
{
  "project": "$name",
  "seeds": ["http://127.0.0.1:$port$seed#entry"],
  "allowed_domains": ["127.0.0.1"],
  "allowed_path_prefixes": ["/docs/"],
  "follow_selector": "main a[href]",
  "parallelism": 3,
  "max_depth": 3,
  "max_pages": 8,
  "max_frontier": 16,
  "max_links_per_page": 20,
  "allow_private_networks": true,
  "user_agent": "HHY-SiteGraph-Self-Test/1.0",
  "root_selector": "html",
  "max_results": 1,
  "schema": {
    "title": { "selector": "head title", "value": "text" },
    "description": { "selector": "meta[name=description]", "value": "attr", "name": "content" },
    "canonical": { "selector": "link[rel=canonical]", "value": "attr", "name": "href" },
    "heading": { "selector": "main h1", "value": "text" }
  }
}
EOF
}

run_case() {
    name=$1
    HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" run "$project_dir/sitegraph.hhy" "$workspace/$name.json" "$workspace/$name-inventory.json" "$workspace/$name-graph.json" "$workspace/$name-report.json" "$workspace/$name-failures.json"
}

write_config healthy /docs/index
write_config risky /docs/risky
HHY_EXTENSION_HOME="$project_dir/.hhy-extensions" "$hhy_bin" check "$project_dir/sitegraph.hhy"
run_case healthy
python3 "$project_dir/test-report.py" healthy "$workspace"

if run_case risky; then
    echo "risky SiteGraph fixture unexpectedly passed" >&2
    exit 1
fi
python3 "$project_dir/test-report.py" risky "$workspace"

cat > "$workspace/ssrf.hhy" <<EOF
http.get("http://127.0.0.1:$port/docs/index", { allow_private_networks: false }) |> send |> print
EOF
if "$hhy_bin" run "$workspace/ssrf.hhy" >"$workspace/ssrf.out" 2>&1; then
    echo "SiteGraph SSRF guard unexpectedly allowed loopback" >&2
    exit 1
fi
grep "private, loopback, or link-local" "$workspace/ssrf.out" >/dev/null
echo "SiteGraph Auditor self-test passed"
