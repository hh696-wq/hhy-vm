#!/usr/bin/env python3
"""Probe optional host capabilities used by the integration test suite."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys


def process_snapshot() -> tuple[bool, str]:
    try:
        result = subprocess.run(
            ["/bin/ps", "-axo", "pid=,pcpu=,rss=,state=,comm="],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=False,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return False, f"process snapshot probe failed: {error}"
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", "replace").strip()
        return False, detail or f"/bin/ps exited with status {result.returncode}"
    return True, "available"


def socket_bind() -> tuple[bool, str]:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
    except OSError as error:
        return False, f"loopback socket bind denied: {error}"
    finally:
        listener.close()
    return True, "available"


PROBES = {
    "process-snapshot": process_snapshot,
    "socket-bind": socket_bind,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capability", choices=sorted(PROBES))
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()
    available, reason = PROBES[args.capability]()
    if not args.quiet:
        state = "available" if available else "unavailable"
        print(f"{args.capability}: {state}: {reason}")
    return 0 if available else 1


if __name__ == "__main__":
    sys.exit(main())
