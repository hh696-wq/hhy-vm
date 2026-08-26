#!/usr/bin/env python3
import csv
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
rows = list(csv.DictReader(Path(sys.argv[2]).open(encoding="utf-8")))

assert report["tool"] == "DataFlow ETL"
assert report["ok"] is True
assert report["source_stats"] == {"qualified_customers": 3, "enriched_customers": 3, "event_files": 2}
assert [item["id"] for item in report["customers"]] == ["101", "102", "105"]
assert report["customers"][0]["email"] == "ada@example.com"
assert report["department_summary"] == [
    {"department": "Engineering", "customers": 2, "total_spend": 730},
    {"department": "Research", "customers": 1, "total_spend": 250},
]
assert len(rows) == 3
assert rows[0]["name"] == "Ada Lovelace"
assert rows[2]["tier"] == "gold"
