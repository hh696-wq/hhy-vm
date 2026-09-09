#!/usr/bin/env python3
import json
import pathlib
import sys

case = sys.argv[1]
root = pathlib.Path(sys.argv[2])
report = json.loads((root / f"{case}-report.json").read_text())
inventory = json.loads((root / f"{case}-inventory.json").read_text())
graph = json.loads((root / f"{case}-graph.json").read_text())
failures = json.loads((root / f"{case}-failures.json").read_text())

assert report["tool"] == "SiteGraph Auditor"
assert report["version"] == "1.0.0"

if case == "healthy":
    assert report["ok"] is True
    assert report["stats"]["requested_pages"] == 4
    assert report["stats"]["successful_pages"] == 4
    assert report["stats"]["failed_pages"] == 0
    assert report["stats"]["records"] == 4
    assert report["stats"]["max_depth_reached"] == 3
    assert report["stats"]["duplicate_urls"] >= 3
    assert report["stats"]["rejected_urls"] == 1
    assert report["stats"]["external_or_rejected_edges"] == 1
    assert [row["title"] for row in inventory] == ["Docs Home", "Guide", "API", "Deep Topic"]
    assert any(edge["to"].endswith("/docs/guide") for edge in graph["edges"])
    assert any(edge["reason"] == "outside_domain" for edge in graph["edges"])
    assert failures == []
else:
    assert report["ok"] is False
    assert report["stats"]["successful_pages"] == 1
    assert report["stats"]["failed_pages"] == 1
    assert report["stats"]["errors"] == 2
    assert report["stats"]["warnings"] == 1
    assert {item["kind"] for item in report["findings"]} == {
        "missing_description", "missing_canonical", "crawl_failure"
    }
    assert len(failures) == 1 and failures[0]["url"].endswith("/docs/missing")
