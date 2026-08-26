#!/usr/bin/env python3
import json
import sys
from pathlib import Path


def verify(path: str, expected_ok: bool, minimum_failures: int) -> None:
    report = json.loads(Path(path).read_text(encoding="utf-8"))
    assert report["tool"] == "FlowGuard"
    assert report["version"] == "1.0.0"
    assert report["ok"] is expected_ok
    assert report["summary"]["total"] == len(report["checks"])
    assert report["summary"]["failed"] >= minimum_failures
    assert report["summary"]["passed"] + report["summary"]["warnings"] + report["summary"]["failed"] == report["summary"]["total"]
    ids = {item["id"] for item in report["checks"]}
    assert len(ids) == len(report["checks"])
    if expected_ok:
        assert "health.fixture-api" in ids
    else:
        assert {"structure.LICENSE", "security.secret.config.js", "command.tests", "health.missing-worker"} <= ids


verify(sys.argv[1], sys.argv[2] == "true", int(sys.argv[3]))
