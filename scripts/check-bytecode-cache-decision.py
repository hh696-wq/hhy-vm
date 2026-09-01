#!/usr/bin/env python3
"""Enforce the evidence gate for the conditional Bytecode cache capability."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--report", default="build/benchmarks/bytecode-cache-decision.json"
    )
    args = parser.parse_args()
    report = json.loads(Path(args.report).read_text(encoding="utf-8"))
    if report.get("schema_version") != 1:
        raise SystemExit("unsupported Bytecode cache decision schema")
    thresholds = report.get("admission_thresholds", {})
    if thresholds.get("minimum_compile_verify_median_ms") != 1.0:
        raise SystemExit("Bytecode cache absolute admission threshold changed without review")
    if thresholds.get("minimum_cold_wall_share") != 0.20:
        raise SystemExit("Bytecode cache relative admission threshold changed without review")
    cases = report.get("measurement", {}).get("cases", {})
    if len(cases) < 5:
        raise SystemExit("Bytecode cache decision lacks the fixed real-workload corpus")
    for name, case in cases.items():
        if case.get("compile_verify", {}).get("iterations", 0) < 15:
            raise SystemExit(f"Bytecode cache case {name} has too few samples")
    decision = report.get("decision", {})
    if decision.get("cache_admitted"):
        raise SystemExit(
            "performance evidence triggered cache admission review; "
            "do not enable a cache without the fingerprint, verifier, invalidation, and threat-model gates"
        )
    expected = {
        "process_cache_enabled": False,
        "disk_cache_enabled": False,
        "external_precompiled_bytecode_accepted": False,
    }
    for field, value in expected.items():
        if decision.get(field) is not value:
            raise SystemExit(f"Bytecode cache decision changed unexpectedly: {field}")
    required = report.get("required_before_future_admission", {})
    if "full_bytecode_verifier" not in required.get("validation", []):
        raise SystemExit("future Bytecode cache policy does not require the full verifier")
    if len(required.get("fingerprint", [])) < 8:
        raise SystemExit("future Bytecode cache fingerprint is incomplete")
    print("Bytecode cache remains rejected by measured cost and governance policy")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
