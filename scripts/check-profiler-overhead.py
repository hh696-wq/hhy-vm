#!/usr/bin/env python3
"""Measure the overhead of observing the verified Stream-kernel path."""

import argparse
import json
from pathlib import Path
import statistics
import subprocess
import tempfile
import time


parser = argparse.ArgumentParser()
parser.add_argument("--binary", default="build/hhy")
parser.add_argument("--iterations", type=int, default=9)
parser.add_argument("--maximum-ratio", type=float, default=1.35)
parser.add_argument("--maximum-delta-ms", type=float, default=12.0)
parser.add_argument("--output", default="build/benchmarks/profiler-overhead.json")
args = parser.parse_args()
source = "tests/valid/bytecode-specialization-profile.hhy"


def measure(command: list[str], expected: str) -> list[float]:
    samples: list[float] = []
    for iteration in range(args.iterations + 2):
        started = time.perf_counter_ns()
        completed = subprocess.run(command, text=True, capture_output=True, check=False)
        elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000
        if completed.returncode != 0 or completed.stdout != expected:
            raise SystemExit(f"benchmark command failed: {' '.join(command)}\n{completed.stderr}")
        if iteration >= 2:
            samples.append(elapsed_ms)
    return samples


expected = "142858\n"
with tempfile.TemporaryDirectory(prefix="hhy-profiler-overhead-") as temporary:
    profile_path = Path(temporary) / "profile.json"
    ordinary_samples = measure([args.binary, "run", source], expected)
    profiled_samples = measure(
        [args.binary, "profile", "--cpu", "--format", "json", "--output",
         str(profile_path), source],
        expected,
    )
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    if not profile.get("optimization_decisions") or not all(
        decision.get("selected") for decision in profile["optimization_decisions"]
    ):
        raise SystemExit("profile benchmark did not execute the verified kernel path")

ordinary_median = statistics.median(ordinary_samples)
profiled_median = statistics.median(profiled_samples)
ratio = profiled_median / ordinary_median
delta_ms = profiled_median - ordinary_median
passed = ratio <= args.maximum_ratio and delta_ms <= args.maximum_delta_ms
report = {
    "schema_version": 1,
    "workload": source,
    "iterations": args.iterations,
    "ordinary_median_ms": round(ordinary_median, 6),
    "profiled_median_ms": round(profiled_median, 6),
    "overhead_ratio": round(ratio, 6),
    "overhead_delta_ms": round(delta_ms, 6),
    "maximum_ratio": args.maximum_ratio,
    "maximum_delta_ms": args.maximum_delta_ms,
    "optimization_path": "verified_stream_kernel",
    "passed": passed,
}
output = Path(args.output)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
if not passed:
    raise SystemExit(
        f"Profiler overhead gate failed: ratio={ratio:.4f}, delta={delta_ms:.3f}ms"
    )
print(f"Profiler overhead gate passed: ratio={ratio:.4f}, delta={delta_ms:.3f}ms")
