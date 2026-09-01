#!/usr/bin/env python3
"""Static guard for internal Runtime module and ownership boundaries."""

from pathlib import Path


runtime = Path("src/runtime.c").read_text(encoding="utf-8")
ownership = Path("src/runtime_ownership.h").read_text(encoding="utf-8")
limits = Path("src/runtime_limits.c").read_text(encoding="utf-8")
bytecode_runtime = Path("src/bytecode_runtime.c").read_text(encoding="utf-8")
bytecode_boundary = Path("src/bytecode_runtime.h").read_text(encoding="utf-8")

required_annotations = (
    "HHY_BORROWED",
    "HHY_MANAGED_SCANNED",
    "HHY_MANAGED_ATOMIC",
    "HHY_NATIVE_OWNED",
)
for annotation in required_annotations:
    if annotation not in ownership:
        raise SystemExit(f"missing ownership annotation: {annotation}")

if "HhyRuntimeLimits hhy_runtime_limits_default" in runtime:
    raise SystemExit("Runtime limits must remain outside the evaluator owner")
if "HhyRuntimeLimits hhy_runtime_limits_default" not in limits:
    raise SystemExit("Runtime limits module does not own its public constructor")
for expected in ("512u * 1024u * 1024u", "256u", "16u", "1000000u"):
    if expected not in limits:
        raise SystemExit(f"default Runtime policy changed or disappeared: {expected}")
if "HHY_MANAGED_SCANNED void *rt_alloc" not in runtime:
    raise SystemExit("scanned managed allocator is not ownership-annotated")
if "HHY_MANAGED_ATOMIC void *rt_alloc_atomic" not in runtime:
    raise SystemExit("atomic managed allocator is not ownership-annotated")
if "runtime_release(HHY_BORROWED Runtime *rt)" not in runtime:
    raise SystemExit("Runtime owner teardown is not ownership-annotated")

for forbidden in (
    "hhy_bytecode_compile(",
    "hhy_bytecode_verify(",
    "hhy_bytecode_prepare_execution(",
):
    if forbidden in runtime:
        raise SystemExit(f"Runtime bypasses the Bytecode boundary: {forbidden}")
for required in (
    "hhy_bytecode_runtime_prepare(",
    "hhy_bytecode_runtime_free(",
    "hhy_bytecode_runtime_chunk(",
    "HHY_BYTECODE_MAX_NESTING + 1u",
):
    if required not in bytecode_runtime:
        raise SystemExit(f"Bytecode Runtime boundary is incomplete: {required}")
if "HHY_BYTECODE_RUNTIME_BOUNDARY_VERSION 4u" not in bytecode_boundary:
    raise SystemExit("Bytecode Runtime internal boundary version changed without review")

for line_number, line in enumerate(runtime.splitlines(), 1):
    if "Value *" in line and ("hhy_alloc(" in line or "hhy_realloc(" in line):
        raise SystemExit(f"managed Value buffer uses native storage at runtime.c:{line_number}")

print("Runtime module and ownership governance checks passed")
