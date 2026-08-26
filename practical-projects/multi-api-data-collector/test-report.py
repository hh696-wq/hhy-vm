#!/usr/bin/env python3
import csv
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    report = json.load(handle)
with open(sys.argv[2], encoding="utf-8") as handle:
    failures = json.load(handle)
with open(sys.argv[3], newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle))

stats = report["stats"]
assert stats["requested_pages"] == 6
assert stats["successful_pages"] == 6
assert stats["failed_pages"] == 0
assert stats["previous_records"] == 9
assert stats["incoming_records"] == 12
assert stats["total_records"] == 9
assert failures == []
assert len(rows) == 9
assert {row["source"] for row in rows} == {"OpenAlex", "Crossref", "GitHub"}
assert all(sum(row["source"] == source for row in rows) == 3 for source in {"OpenAlex", "Crossref", "GitHub"})
assert len({(row["source"], row["external_id"]) for row in rows}) == 9
assert rows == sorted(rows, key=lambda row: f'{row["source"]}:{row["external_id"]}')
print("verified: 6 pages, 12 incoming records, 9 unique records")
