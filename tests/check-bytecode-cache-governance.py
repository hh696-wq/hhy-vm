#!/usr/bin/env python3
"""Verify metrics and the fail-closed no-cache boundary for v1.3.10."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


binary = sys.argv[1] if len(sys.argv) > 1 else "build/hhy"
metrics = subprocess.run(
    [binary, "bytecode", "--metrics", "examples/00-hello.hhy"],
    capture_output=True,
    text=True,
    check=True,
)
report = json.loads(metrics.stdout)
required = {
    "schema_version",
    "bytecode_format_version",
    "stream_kernel_version",
    "source_bytes",
    "instructions",
    "constants",
    "stream_kernels",
    "compile_verify_ns",
    "verify_prepare_ns",
}
missing = required - report.keys()
if missing:
    raise SystemExit(f"Bytecode metrics are incomplete: {sorted(missing)}")
if report["compile_verify_ns"] <= 0 or report["verify_prepare_ns"] <= 0:
    raise SystemExit("Bytecode metrics must report positive preparation timings")

with tempfile.TemporaryDirectory(prefix="hhy-untrusted-bytecode-") as directory:
    artifact = Path(directory) / "untrusted.hhyc"
    artifact.write_bytes(b"HHYBC\x00\xffunverified external bytecode")
    attempted = subprocess.run(
        [binary, "run", str(artifact)], capture_output=True, text=True, check=False
    )
    if attempted.returncode == 0:
        raise SystemExit("unverified external Bytecode was accepted for execution")

public_header = Path("include/hhy/bytecode.h").read_text(encoding="utf-8")
runtime_boundary = Path("src/bytecode_runtime.h").read_text(encoding="utf-8")
for forbidden in (
    "hhy_bytecode_deserialize",
    "hhy_bytecode_load_cache",
    "hhy_bytecode_execute_external",
):
    if forbidden in public_header or forbidden in runtime_boundary:
        raise SystemExit(f"unreviewed external Bytecode loader entered the boundary: {forbidden}")

print("Bytecode cache governance and unverified-artifact rejection checks passed")
