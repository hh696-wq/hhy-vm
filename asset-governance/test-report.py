#!/usr/bin/env python3
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
kinds = {item["kind"] for item in report["findings"]}

assert report["tool"] == "Asset Governance"
assert report["ok"] is False
assert {"large_file", "naming", "stale", "sensitive", "duplicate_content"} <= kinds
assert report["summary"]["critical"] >= 1
assert report["summary"]["planned_actions"] == 3
assert [action["kind"] for action in report["actions"]] == ["copy", "move", "remove"]
