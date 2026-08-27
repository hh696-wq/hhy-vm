#!/usr/bin/env python3
import json
import sys

records = json.load(open(sys.argv[1], encoding="utf-8"))
report = json.load(open(sys.argv[2], encoding="utf-8"))
failures = json.load(open(sys.argv[3], encoding="utf-8"))
assert [item["title"] for item in records] == ["First item", "Second item"]
assert [item["anchor"] for item in records] == ["one", "two"]
assert all(item["source_url"].startswith("http://127.0.0.1:") for item in records)
assert report["ok"] is True and report["stats"]["records"] == 2
assert failures == []
