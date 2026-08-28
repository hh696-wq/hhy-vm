#!/usr/bin/env python3
import json
import sys

records = json.load(open(sys.argv[1], encoding="utf-8"))
report = json.load(open(sys.argv[2], encoding="utf-8"))
failures = json.load(open(sys.argv[3], encoding="utf-8"))
assert [item["title"] for item in records] == ["First item", "Second item", "Third item"]
assert [item["anchor"] for item in records] == ["one", "two", "three"]
assert all(item["source_url"].startswith("http://127.0.0.1:") for item in records)
assert report["ok"] is True and report["version"] == "2.0.0"
assert report["stats"]["records"] == 3
assert report["stats"]["requested_pages"] == 3
assert report["stats"]["max_depth_reached"] == 2
assert report["stats"]["duplicate_urls"] >= 2
assert report["stats"]["rejected_urls"] == 1
assert report["stats"]["unique_fingerprints"] == 3
assert failures == []
