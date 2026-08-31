#!/usr/bin/env python3
"""Run the reproducible v1.1 performance baseline and write JSON evidence."""

from __future__ import annotations

import argparse
import json
import os
import platform
import statistics
import subprocess
import time
from pathlib import Path


def git_revision() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], capture_output=True, text=True, check=False
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def run_case(command: list[str], iterations: int, expected: str | None) -> dict[str, object]:
    samples: list[float] = []
    for _ in range(iterations):
        started = time.perf_counter_ns()
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000
        if result.returncode != 0:
            raise RuntimeError(
                f"benchmark command failed ({result.returncode}): {' '.join(command)}\n{result.stderr}"
            )
        if expected is not None and result.stdout.strip() != expected:
            raise RuntimeError(
                f"benchmark output mismatch for {' '.join(command)}: {result.stdout!r}"
            )
        samples.append(round(elapsed_ms, 3))
    return {
        "iterations": iterations,
        "median_ms": round(statistics.median(samples), 3),
        "min_ms": min(samples),
        "max_ms": max(samples),
        "samples_ms": samples,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/hhy")
    parser.add_argument("--iterations", type=int, default=5)
    parser.add_argument("--output", default="build/benchmarks/baseline.json")
    args = parser.parse_args()
    if args.iterations < 3:
        parser.error("--iterations must be at least 3")

    binary = str(Path(args.binary).resolve())
    cases = {
        "cli_version": ([binary, "--version"], None),
        "basic_flow": ([binary, "run", "tests/valid/advanced-flow.hhy"], None),
        "core_flow_100k": ([binary, "run", "benchmarks/core-flow.hhy"], "33334"),
        "json_flow": ([binary, "run", "tests/valid/json-flow.hhy"], '["Ada", "Grace"]'),
    }
    results = {
        name: run_case(command, args.iterations, expected)
        for name, (command, expected) in cases.items()
    }
    report = {
        "schema_version": 1,
        "hhy_version": Path("VERSION").read_text(encoding="utf-8").strip(),
        "git_revision": git_revision(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "cpu_count": os.cpu_count(),
        "results": results,
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote HHY performance baseline to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
