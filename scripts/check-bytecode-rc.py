#!/usr/bin/env python3
"""Evaluate the evidence-driven Bytecode default-engine switch gate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", default="build/benchmarks/engine-comparison.json")
    parser.add_argument("--output", default="build/benchmarks/bytecode-rc-decision.json")
    parser.add_argument("--require-ready", action="store_true")
    args = parser.parse_args()

    report = json.loads(Path(args.report).read_text(encoding="utf-8"))
    if report.get("schema_version") != 2:
        raise SystemExit("Bytecode RC evaluation requires benchmark schema 2")
    comparisons = report["comparisons"]
    gates = {
        "cpu_bound_substantial_gain": {
            "actual": comparisons["core_flow_1m"]["bytecode_to_ast_wall_ratio"],
            "maximum": 0.90,
        },
        "short_script_no_material_regression": {
            "actual": comparisons["basic_flow"]["bytecode_to_ast_wall_ratio"],
            "maximum": 1.25,
        },
        "io_json_no_material_regression": {
            "actual": comparisons["json_flow"]["bytecode_to_ast_wall_ratio"],
            "maximum": 1.10,
        },
    }
    for gate in gates.values():
        gate["passed"] = gate["actual"] <= gate["maximum"]
    ready = all(gate["passed"] for gate in gates.values())
    decision = {
        "schema_version": 1,
        "benchmark_git_revision": report["git_revision"],
        "ready_for_default_bytecode": ready,
        "selected_default_engine": "bytecode" if ready else "ast",
        "gates": gates,
        "reason": (
            "all measured RC performance gates passed"
            if ready
            else "Bytecode remains opt-in until every measured RC performance gate passes"
        ),
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(decision, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(decision["reason"])
    for name, gate in gates.items():
        print(f"- {name}: ratio={gate['actual']:.4f}, maximum={gate['maximum']:.2f}, passed={gate['passed']}")
    return 1 if args.require_ready and not ready else 0


if __name__ == "__main__":
    raise SystemExit(main())
