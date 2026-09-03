#!/usr/bin/env python3
"""End-to-end acceptance coverage for the persistent HHY Web Runtime."""

from __future__ import annotations

import http.client
import gzip
import json
import os
from pathlib import Path
import signal
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def reserve_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def request(
    port: int,
    method: str,
    path: str,
    body: bytes = b"",
    headers: dict[str, str] | None = None,
) -> tuple[int, dict[str, str], bytes]:
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
    request_headers = {"Content-Length": str(len(body)), **(headers or {})}
    connection.request(method, path, body=body, headers=request_headers)
    response = connection.getresponse()
    result = response.status, {key.lower(): value for key, value in response.getheaders()}, response.read()
    connection.close()
    return result


def main() -> int:
    binary = sys.argv[1] if len(sys.argv) > 1 else "build/hhy"
    port = reserve_port()
    with tempfile.TemporaryDirectory(prefix="hhy-web-") as directory:
        root = Path(directory)
        (root / "hello.txt").write_text("static works\n", encoding="utf-8")
        (root / "large.bin").write_bytes(b"0123456789abcdef" * (128 * 1024))
        source = root / "web-server.hhy"
        shutil.copyfile("tests/acceptance/web-server.hhy", source)
        process = subprocess.Popen(
            [binary, "serve", "--dev", str(source), "--", str(port), str(root)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            start_new_session=True,
        )
        failure_status = 0
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise AssertionError(process.stderr.read())
                try:
                    status, _, body = request(port, "GET", "/")
                    if status == 200:
                        break
                except OSError:
                    time.sleep(0.05)
            else:
                raise AssertionError("HHY Web Server did not become ready")

            assert json.loads(body) == {"ok": True, "path": "/"}
            status, headers, body = request(
                port, "GET", "/users/42", headers={"Origin": "https://example.com"}
            )
            assert status == 200 and json.loads(body) == {"id": "42"}
            assert headers["access-control-allow-origin"] == "https://example.com"
            assert headers["vary"] == "Origin" and headers["x-request-id"]
            status, headers, _ = request(
                port, "OPTIONS", "/users/42",
                headers={"Origin": "https://example.com", "Access-Control-Request-Headers": "X-Test"},
            )
            assert status == 204 and "GET" in headers["access-control-allow-methods"]
            assert headers["access-control-allow-headers"] == "X-Test"
            status, headers, body = request(port, "POST", "/echo", b"hello")
            assert status == 201 and headers["x-hhy-test"] == "yes" and body == b"hello"
            assert request(port, "GET", "/echo")[0] == 405
            assert request(port, "GET", "/missing")[0] == 404
            assert request(port, "GET", "/blocked")[:1] == (403,)
            status, headers, body = request(port, "GET", "/assets/hello.txt")
            assert status == 200 and headers["content-type"].startswith("text/plain")
            assert body == b"static works\n"
            assert headers["etag"] and headers["last-modified"]
            assert request(
                port, "GET", "/assets/hello.txt", headers={"If-None-Match": headers["etag"]}
            )[0] == 304
            status, headers, body = request(
                port, "GET", "/assets/hello.txt", headers={"Range": "bytes=0-5"}
            )
            assert status == 206 and headers["content-range"] == "bytes 0-5/13"
            assert headers["accept-ranges"] == "bytes" and body == b"static"
            assert request(
                port, "GET", "/assets/hello.txt", headers={"Range": "bytes=99-100"}
            )[0] == 416
            status, headers, body = request(port, "GET", "/assets/large.bin")
            assert status == 200 and headers["transfer-encoding"] == "chunked"
            assert len(body) == 2 * 1024 * 1024 and body[:16] == b"0123456789abcdef"
            boundary = "hhy-test-boundary"
            upload = (
                f"--{boundary}\r\nContent-Disposition: form-data; name=\"title\"\r\n\r\n"
                f"report\r\n--{boundary}\r\n"
                f"Content-Disposition: form-data; name=\"document\"; filename=\"report.txt\"\r\n"
                f"Content-Type: text/plain\r\n\r\nhello upload\r\n--{boundary}--\r\n"
            ).encode()
            status, _, body = request(
                port,
                "POST",
                "/upload",
                upload,
                {"Content-Type": f"multipart/form-data; boundary={boundary}"},
            )
            assert status == 200
            assert json.loads(body) == {
                "title": "report",
                "filename": "report.txt",
                "size": 12,
                "content": "hello upload",
            }
            assert not list(root.glob("hhy-upload-*")), "upload temporary file leaked"
            status, headers, body = request(port, "GET", "/stream")
            assert status == 200 and headers["transfer-encoding"] == "chunked"
            assert body == b"one-two"
            status, headers, body = request(port, "GET", "/events")
            assert status == 200 and headers["content-type"].startswith("text/event-stream")
            assert body == b"data: first\n\ndata: second\n\n"
            status, headers, body = request(port, "GET", "/cookie")
            assert status == 200 and body == b"cookie set"
            cookie = headers["set-cookie"].split(";", 1)[0]
            status, _, body = request(port, "GET", "/cookie/read", headers={"Cookie": cookie})
            assert status == 200 and json.loads(body) == {"value": "user-42"}, (status, body, cookie)
            tampered = cookie[:-1] + ("0" if cookie[-1] != "0" else "1")
            status, _, body = request(port, "GET", "/cookie/read", headers={"Cookie": tampered})
            assert status == 200 and json.loads(body) == {"value": None}
            status, headers, body = request(port, "GET", "/metrics")
            assert status == 200 and "version=0.0.4" in headers["content-type"]
            assert b"hhy_web_requests_total" in body
            assert b"hhy_web_handler_seconds_total" in body
            status, _, body = request(port, "GET", "/healthz")
            assert status == 200 and json.loads(body) == {"status": "ok"}
            status, headers, body = request(
                port, "GET", "/gzip", headers={"Accept-Encoding": "gzip"}
            )
            assert status == 200 and headers["content-encoding"] == "gzip"
            assert gzip.decompress(body).startswith(b"HHY Web Runtime compression")
            status, _, body = request(port, "GET", "/query?name=HHY+Runtime&lang=hhy")
            assert status == 200 and json.loads(body) == {"name": "HHY Runtime", "lang": "hhy"}
            assert request(port, "GET", "/explode")[0] == 500
            assert request(port, "GET", "/")[0] == 200, "handler failure stopped the worker"
            assert request(port, "GET", "/assets/../outside.txt")[0] == 404
            large = b"x" * 1025
            assert request(port, "POST", "/echo", large)[0] == 413
            source.touch()
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                try:
                    status, _, body = request(port, "GET", "/")
                    if status == 200 and json.loads(body)["ok"] is True:
                        break
                except OSError:
                    pass
                time.sleep(0.05)
            else:
                raise AssertionError("development hot reload did not restore the Web application")
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGINT)
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=3)
            try:
                stdout, stderr = process.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                stdout, stderr = process.communicate(timeout=3)
            if stdout:
                sys.stdout.write(stdout)
            if process.returncode not in (0, -signal.SIGINT):
                sys.stderr.write(stderr)
                failure_status = process.returncode or 1
        if failure_status:
            return failure_status
    print("HHY Web Server acceptance tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
