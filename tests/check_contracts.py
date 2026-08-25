#!/usr/bin/env python3
"""Keep callable implementations and the single internal Registry in lock-step."""

import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
runtime = (root / "src/runtime.c").read_text(encoding="utf-8")
registry = (root / "src/contracts.c").read_text(encoding="utf-8")

implemented = set(re.findall(r'strcmp\(name,\s*"([^"]+)"\)', runtime))
registered = set(re.findall(r'C\("([^"]+)"', registry))
details = re.findall(
    r'C\("([^"]+)".*?\n\s+"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\)',
    registry,
    flags=re.DOTALL,
)

missing = sorted(implemented - registered)
orphaned = sorted(registered - implemented)
if missing or orphaned:
    if missing:
        print("callables missing Registry contracts: " + ", ".join(missing), file=sys.stderr)
    if orphaned:
        print("Registry contracts missing Runtime implementations: " + ", ".join(orphaned), file=sys.stderr)
    raise SystemExit(1)

if len(details) != len(registered):
    print(
        f"Registry metadata is incomplete: {len(details)} detailed contracts for "
        f"{len(registered)} callables",
        file=sys.stderr,
    )
    raise SystemExit(1)

allowed_threading = {"main", "worker", "isolated_process"}
for name, input_contract, output_contract, threading in details:
    if input_contract == "runtime-checked" or not input_contract or not output_contract:
        print(f"Registry contract {name} uses placeholder type metadata", file=sys.stderr)
        raise SystemExit(1)
    if threading not in allowed_threading:
        print(f"Registry contract {name} has invalid threading model {threading}", file=sys.stderr)
        raise SystemExit(1)

print(f"validated {len(registered)} callable contracts")
