#!/usr/bin/env python3
import sys
from pathlib import Path

root = Path(sys.argv[1])
phase = sys.argv[2]

if phase == "dry-run":
    assert (root / "assets/Logo Final.PNG").exists()
    assert not (root / "quarantine/Logo Final.PNG").exists()
    assert (root / "tmp/old.tmp").exists()
    assert not (root / "archive/old.tmp").exists()
    assert (root / "build/stale.bundle.js").exists()
else:
    assert (root / "assets/Logo Final.PNG").exists()
    assert (root / "quarantine/Logo Final.PNG").exists()
    assert not (root / "tmp/old.tmp").exists()
    assert (root / "archive/old.tmp").exists()
    assert not (root / "build/stale.bundle.js").exists()
