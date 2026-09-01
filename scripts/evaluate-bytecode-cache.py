#!/usr/bin/env python3
"""Measure whether Bytecode compile/verify cost justifies a cache attack surface."""

from __future__ import annotations

import argparse
import json
import os
import platform
import statistics
import subprocess
import time
from pathlib import Path


MIN_COMPILE_MS = 1.0
MIN_COLD_WALL_SHARE = 0.20


def git_revision() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], capture_output=True, text=True, check=False
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def run(command: list[str]) -> tuple[float, str]:
    started = time.perf_counter_ns()
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000
    if result.returncode != 0:
        raise RuntimeError(
            f"cache evidence command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stderr}"
        )
    return elapsed_ms, result.stdout


def summary(samples: list[float]) -> dict[str, object]:
    return {
        "iterations": len(samples),
        "median_ms": round(statistics.median(samples), 6),
        "min_ms": round(min(samples), 6),
        "max_ms": round(max(samples), 6),
        "samples_ms": [round(sample, 6) for sample in samples],
    }


def measure_case(binary: str, source: str, iterations: int) -> dict[str, object]:
    metrics_command = [binary, "bytecode", "--metrics", source]
    run_command = [binary, "run", "--engine", "bytecode", source]
    for _ in range(2):
        run(metrics_command)
        run(run_command)

    compile_samples: list[float] = []
    prepare_samples: list[float] = []
    cold_wall_samples: list[float] = []
    metadata: dict[str, object] | None = None
    for iteration in range(iterations):
        commands = (metrics_command, run_command)
        if iteration % 2:
            commands = (run_command, metrics_command)
        for command in commands:
            wall_ms, output = run(command)
            if command is metrics_command:
                current = json.loads(output)
                if current.get("schema_version") != 1:
                    raise RuntimeError("unsupported Bytecode metrics schema")
                metadata = current
                compile_samples.append(current["compile_verify_ns"] / 1_000_000)
                prepare_samples.append(current["verify_prepare_ns"] / 1_000_000)
            else:
                cold_wall_samples.append(wall_ms)

    assert metadata is not None
    compile_median = statistics.median(compile_samples)
    cold_median = statistics.median(cold_wall_samples)
    share = compile_median / cold_median if cold_median > 0 else 1.0
    trigger = compile_median >= MIN_COMPILE_MS and share >= MIN_COLD_WALL_SHARE
    return {
        "source": source,
        "source_bytes": metadata["source_bytes"],
        "instructions": metadata["instructions"],
        "constants": metadata["constants"],
        "stream_kernels": metadata["stream_kernels"],
        "compile_verify": summary(compile_samples),
        "verify_prepare": summary(prepare_samples),
        "cold_bytecode_run": summary(cold_wall_samples),
        "compile_verify_cold_wall_share": round(share, 6),
        "cache_admission_triggered": trigger,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/hhy")
    parser.add_argument("--iterations", type=int, default=21)
    parser.add_argument(
        "--output", default="build/benchmarks/bytecode-cache-decision.json"
    )
    args = parser.parse_args()
    if args.iterations < 15:
        parser.error("--iterations must be at least 15")

    binary = str(Path(args.binary).resolve())
    sources = (
        "examples/00-hello.hhy",
        "tests/valid/advanced-flow.hhy",
        "tests/valid/stdlib.hhy",
        "benchmarks/json-flow.hhy",
        "benchmarks/core-flow-1m.hhy",
    )
    cases = {Path(source).stem: measure_case(binary, source, args.iterations)
             for source in sources}
    triggered = [name for name, case in cases.items()
                 if case["cache_admission_triggered"]]
    admitted = bool(triggered)
    report = {
        "schema_version": 1,
        "hhy_version": Path("VERSION").read_text(encoding="utf-8").strip(),
        "git_revision": git_revision(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "cpu_count": os.cpu_count(),
        "measurement": {
            "method": "paired fresh-process compile metrics and Bytecode runs",
            "cache_read_cost_assumption": "zero (optimistic upper bound)",
            "cases": cases,
        },
        "admission_thresholds": {
            "minimum_compile_verify_median_ms": MIN_COMPILE_MS,
            "minimum_cold_wall_share": MIN_COLD_WALL_SHARE,
            "trigger_rule": "any fixed real workload must meet both thresholds",
        },
        "decision": {
            "cache_admitted": admitted,
            "triggered_cases": triggered,
            "reason": (
                "compile_verify_is_a_major_measured_cost"
                if admitted else "compile_verify_not_a_major_measured_cost"
            ),
            "process_cache_enabled": False,
            "disk_cache_enabled": False,
            "external_precompiled_bytecode_accepted": False,
        },
        "required_before_future_admission": {
            "fingerprint": [
                "source_sha256",
                "recursive_dependency_sha256",
                "hhy_version",
                "bytecode_format_version",
                "stream_kernel_version",
                "compiler_features",
                "target_triple",
                "security_policy_version",
            ],
            "validation": [
                "checksum",
                "bounded_decode",
                "full_bytecode_verifier",
                "execution_plan_verifier",
            ],
            "failure_policy": "stale, damaged, unknown, or unverified data is ignored and recompiled",
            "storage_policy": "atomic writes, bounded size, and no untrusted symlink traversal",
        },
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        f"wrote Bytecode cache decision to {output}: "
        f"{'admission review required' if admitted else 'cache not admitted'}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
