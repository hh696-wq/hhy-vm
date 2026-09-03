#!/usr/bin/env python3
"""Bounded concurrent short-request stress test for HHY Web Runtime."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import http.client
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import tempfile
import time


def reserve_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def one_request(port: int) -> bool:
    try:
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        connection.request("GET", "/")
        response = connection.getresponse()
        body = response.read()
        connection.close()
        return response.status == 200 and json.loads(body) == {"ok": True, "path": "/"}
    except (OSError, ValueError):
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/hhy")
    parser.add_argument("--requests", type=int, default=1_000_000)
    parser.add_argument("--clients", type=int, default=16)
    args = parser.parse_args()
    if args.requests < 1 or not 1 <= args.clients <= 128:
        parser.error("requests must be positive and clients must be from 1 to 128")

    port = reserve_port()
    with tempfile.TemporaryDirectory(prefix="hhy-web-stress-") as directory:
        root = Path(directory)
        source = root / "web-server.hhy"
        shutil.copyfile("tests/acceptance/web-server.hhy", source)
        environment = {**os.environ, "HHY_WEB_ACCESS_LOG": "0"}
        process = subprocess.Popen(
            [args.binary, "serve", str(source), "--", str(port), str(root)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=environment,
            start_new_session=True,
        )
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and not one_request(port):
                if process.poll() is not None:
                    raise RuntimeError("HHY Web stress server exited during startup")
                time.sleep(0.05)
            else:
                if time.monotonic() >= deadline:
                    raise RuntimeError("HHY Web stress server did not become ready")
            started = time.monotonic()
            with ThreadPoolExecutor(max_workers=args.clients) as executor:
                results = executor.map(lambda _: one_request(port), range(args.requests), chunksize=256)
                failures = sum(1 for result in results if not result)
            elapsed = time.monotonic() - started
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)
    rate = args.requests / elapsed
    print(f"HHY Web stress: requests={args.requests} clients={args.clients} failures={failures} seconds={elapsed:.3f} requests_per_second={rate:.1f}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
