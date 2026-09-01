#!/usr/bin/env python3
"""Fail CI when a reproducible HHY benchmark exceeds its reviewed budget."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", default="build/benchmarks/baseline.json")
    parser.add_argument("--budget", default="benchmarks/performance-budget.json")
    args = parser.parse_args()
    report = json.loads(Path(args.report).read_text(encoding="utf-8"))
    budget = json.loads(Path(args.budget).read_text(encoding="utf-8"))
    if report.get("schema_version") not in {1, 2} or budget.get("schema_version") != 1:
        raise SystemExit("unsupported performance evidence schema")
    results = report["results"]
    failures: list[str] = []
    for name, ceiling in budget["maximum_median_ms"].items():
        actual = results[name]["median_ms"]
        if actual > ceiling:
            failures.append(f"{name}: {actual:.3f} ms exceeds {ceiling:.3f} ms")
    startup = results["cli_version"]["median_ms"]
    if startup <= 0:
        failures.append("cli_version: median must be positive")
    else:
        for name, ceiling in budget["maximum_startup_ratio"].items():
            ratio = results[name]["median_ms"] / startup
            if ratio > ceiling:
                failures.append(f"{name}: startup ratio {ratio:.2f} exceeds {ceiling:.2f}")
    if failures:
        print("performance regression gate failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print(f"performance regression gate passed for {len(results)} benchmarks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
