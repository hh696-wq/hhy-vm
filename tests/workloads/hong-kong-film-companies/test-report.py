#!/usr/bin/env python3

import csv
import json
import sys
from pathlib import Path


report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
with Path(sys.argv[2]).open(encoding="utf-8", newline="") as handle:
    rows = list(csv.DictReader(handle))

assert report["ok"] is True
assert report["stats"] == {
    "search_results": 5,
    "fetched_pages": 5,
    "failed_pages": 0,
    "qualified_companies": 3,
}
assert [row["company"] for row in rows] == ["天下一電影", "橙天嘉禾", "邵氏兄弟"]
assert all(row["wikipedia_url"].startswith("https://zh.wikipedia.org/wiki/") for row in rows)
assert all(row["summary"] for row in rows)
