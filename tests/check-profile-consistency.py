#!/usr/bin/env python3
"""Verify that profiling observes, but does not change, Bytecode optimization."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = sys.argv[1] if len(sys.argv) > 1 else "build/hhy"
source = "tests/valid/bytecode-specialization.hhy"
cancel_source = "tests/valid/bytecode-specialization-cancel.hhy"


def execute(command: list[str], environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, env=environment, check=False)


def events(stderr: str) -> list[dict[str, object]]:
    return [json.loads(line) for line in stderr.splitlines() if line.startswith("{")]


with tempfile.TemporaryDirectory(prefix="hhy-profile-consistency-") as temporary:
    profile_path = Path(temporary) / "profile.json"
    environment = os.environ.copy()
    environment["HHY_BYTECODE_SPECIALIZATION_REPORT"] = "json"
    ordinary = execute([binary, "run", source], environment)
    profiled = execute(
        [binary, "profile", "--heap", "--format", "json", "--output",
         str(profile_path), source],
        environment,
    )
    if ordinary.returncode != 0 or profiled.returncode != 0:
        raise SystemExit("ordinary or profiled specialization execution failed")
    if ordinary.stdout != profiled.stdout:
        raise SystemExit("Profiler changed specialization stdout")
    ordinary_events = events(ordinary.stderr)
    profiled_events = events(profiled.stderr)
    if ordinary_events != profiled_events or not ordinary_events:
        raise SystemExit("Profiler changed the optimization decisions")
    if any(not event.get("selected") or event.get("reason") != "selected"
           for event in ordinary_events):
        raise SystemExit("verified kernels were not selected")

    report = json.loads(profile_path.read_text(encoding="utf-8"))
    if report.get("schema_version") != 2 or report.get("engine") != "bytecode":
        raise SystemExit("profile report schema or engine is incorrect")
    if report.get("optimization_decisions_dropped") != 0:
        raise SystemExit("profile unexpectedly dropped optimization decisions")
    decisions = report.get("optimization_decisions")
    if not isinstance(decisions, list) or len(decisions) != len(ordinary_events):
        raise SystemExit("profile report omitted optimization decisions")
    for decision, event in zip(decisions, ordinary_events, strict=True):
        operations = [stage["operation"] for stage in decision["stages"]]
        if (decision["selected"], decision["reason"], operations) != (
            event["selected"], event["reason"], event["operations"]
        ):
            raise SystemExit("profile decision metadata differs from Runtime selection")
        kernels = [stage for stage in decision["stages"] if stage["kernel_version"]]
        if not kernels or any(stage["kernel_version"] != 1 or not stage["opcodes"]
                              for stage in kernels):
            raise SystemExit("profile report omitted versioned kernel opcodes")

    hotspots = report.get("hotspots", [])
    if not any(item["name"].startswith("<stream-kernel:") and item["calls"] > 0
               for item in hotspots):
        raise SystemExit("profile hotspots omitted Stream kernel execution")
    if not any(item["name"] == "<stream-collect>" and
               item["allocated_bytes"] > 0 for item in hotspots):
        raise SystemExit("profile hotspots omitted fused collect Heap attribution")

    disabled_environment = environment.copy()
    disabled_environment["HHY_BYTECODE_SPECIALIZATION"] = "off"
    disabled_profile = Path(temporary) / "disabled.json"
    disabled = execute(
        [binary, "profile", "--format", "json", "--output", str(disabled_profile), source],
        disabled_environment,
    )
    if disabled.returncode != 0:
        raise SystemExit("disabled specialization profile failed")
    disabled_report = json.loads(disabled_profile.read_text(encoding="utf-8"))
    if any(decision["selected"] or decision["reason"] != "disabled"
           for decision in disabled_report["optimization_decisions"]):
        raise SystemExit("profile report did not preserve the disabled decision")

    for command in ("run", "profile"):
        arguments = [binary, command]
        if command == "profile":
            arguments.extend(["--format", "json", "--output",
                              str(Path(temporary) / "cancel.json")])
        arguments.extend(["--limit", "max_runtime=1ms", cancel_source])
        cancelled = execute(arguments, environment)
        if cancelled.returncode != 5 or "TimeoutError" not in cancelled.stderr:
            raise SystemExit(f"{command} did not preserve fused cancellation semantics")

print("Profiler optimization, kernel, Heap, and cancellation consistency tests passed")
