#!/usr/bin/env python3
"""Run official real-workload self-tests under both execution engines."""

from __future__ import annotations

import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def socket_available() -> bool:
    probe = subprocess.run(
        ["python3", "tests/capabilities.py", "socket-bind", "--quiet"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return probe.returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/hhy")
    parser.add_argument("--output", default="build/benchmarks/bytecode-workloads.json")
    args = parser.parse_args()

    binary = str((ROOT / args.binary).resolve())
    local_cases = ["asset-governance"]
    network_cases = [
        "dataflow-etl",
        "flowguard",
        "hong-kong-film-companies",
        "multi-api-data-collector",
        "sitegraph-auditor",
    ]
    browser_cases = ["my-crawler"] if os.environ.get("HHY_WORKLOAD_BROWSER") == "1" else []
    has_socket = socket_available()
    cases = local_cases + (network_cases if has_socket else []) + (browser_cases if has_socket else [])
    skipped = ([] if has_socket else network_cases) + ([] if browser_cases else ["my-crawler"])
    results: dict[str, dict[str, object]] = {}
    failed = False
    for engine in ("ast", "bytecode"):
        engine_results: dict[str, object] = {}
        for case in cases:
            command = ["sh", f"practical-projects/{case}/self-test.sh"]
            environment = os.environ.copy()
            environment.update({"HHY_BIN": binary, "HHY_ENGINE": engine})
            started = time.perf_counter()
            run = subprocess.run(
                command, cwd=ROOT, env=environment, capture_output=True,
                text=True, check=False, timeout=180,
            )
            elapsed = round(time.perf_counter() - started, 3)
            engine_results[case] = {
                "status": run.returncode,
                "elapsed_seconds": elapsed,
                "stdout_tail": run.stdout.strip().splitlines()[-1:] or [],
                "stderr_tail": run.stderr.strip().splitlines()[-3:] or [],
            }
            if run.returncode != 0:
                failed = True
                print(f"{engine} workload failed: {case}\n{run.stderr}")
            else:
                print(f"{engine} workload passed: {case} ({elapsed:.3f}s)")
        results[engine] = engine_results

    for case in cases:
        if results["ast"][case]["status"] != results["bytecode"][case]["status"]:
            failed = True
            print(f"engine workload status mismatch: {case}")
    report = {
        "schema_version": 1,
        "hhy_version": (ROOT / "VERSION").read_text(encoding="utf-8").strip(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "binary": binary,
        "results": results,
        "skipped_for_missing_socket_capability": skipped,
        "passed": not failed,
    }
    output = ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
