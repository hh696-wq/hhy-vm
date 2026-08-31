#!/usr/bin/env python3
"""Merge same-root, signed target Registry archives into one BT deployment archive."""

import base64
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path
import argparse


PROJECT = Path(__file__).resolve().parents[1]
VERSION = (PROJECT / "VERSION").read_text().strip()
PRIVATE_KEY = PROJECT / ".hhy-private" / "official-registry-ed25519.pem"


def openssl_command() -> str:
    configured = os.environ.get("HHY_OPENSSL")
    if configured:
        return configured
    try:
        prefix = subprocess.run(
            ["brew", "--prefix", "openssl@3"], check=True,
            capture_output=True, text=True,
        ).stdout.strip()
        candidate = Path(prefix) / "bin" / "openssl"
        if candidate.is_file():
            return str(candidate)
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    return "openssl"


OPENSSL = openssl_command()


def canonical(value: dict) -> bytes:
    return json.dumps(value, ensure_ascii=True, separators=(",", ":"), sort_keys=True).encode()


def sign(value: dict) -> str:
    with tempfile.TemporaryDirectory(prefix="hhy-registry-merge-sign-") as temporary:
        payload = Path(temporary) / "payload.json"
        signature = Path(temporary) / "signature.bin"
        payload.write_bytes(canonical(value))
        subprocess.run(
            [OPENSSL, "pkeyutl", "-sign", "-rawin", "-inkey", str(PRIVATE_KEY),
             "-in", str(payload), "-out", str(signature)],
            check=True,
        )
        return base64.b64encode(signature.read_bytes()).decode()


def safe_extract(archive_path: Path, destination: Path) -> None:
    with tarfile.open(archive_path, "r:gz") as archive:
        for member in archive.getmembers():
            path = Path(member.name)
            if path.is_absolute() or ".." in path.parts or member.issym() or member.islnk():
                raise RuntimeError(f"unsafe archive member in {archive_path}: {member.name}")
        archive.extractall(destination)


def public_key_from_private() -> str:
    with tempfile.TemporaryDirectory(prefix="hhy-registry-merge-public-") as temporary:
        public_der = Path(temporary) / "public.der"
        subprocess.run(
            [OPENSSL, "pkey", "-in", str(PRIVATE_KEY), "-pubout", "-outform", "DER",
             "-out", str(public_der)],
            check=True,
        )
        return base64.b64encode(public_der.read_bytes()[-32:]).decode()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, default=PROJECT / "build" / "registry")
    args = parser.parse_args()
    if not PRIVATE_KEY.is_file():
        raise SystemExit(f"missing signing key: {PRIVATE_KEY}")

    output = args.output.resolve()
    site = output / "merged-site"
    if site.exists():
        shutil.rmtree(site)
    site.mkdir(parents=True)
    packages = []
    coordinates = set()
    expected_root = None

    with tempfile.TemporaryDirectory(prefix="hhy-registry-merge-") as temporary:
        temporary_root = Path(temporary)
        for archive_number, archive_path in enumerate(args.archives):
            extracted = temporary_root / str(archive_number)
            extracted.mkdir()
            safe_extract(archive_path.resolve(), extracted)
            root = json.loads((extracted / "root.json").read_text())
            if expected_root is None:
                expected_root = root
            elif root != expected_root:
                raise RuntimeError("Registry snapshots do not share the same trust root")
            index = json.loads((extracted / "index.json").read_text())
            for package in index.get("packages", []):
                coordinate = (package["identity"], package["version"], package["target"])
                if coordinate in coordinates:
                    raise RuntimeError(f"duplicate package target: {coordinate}")
                coordinates.add(coordinate)
                source = package["source"]
                source_path = extracted / source
                target_path = site / source
                if not source_path.is_dir():
                    raise RuntimeError(f"missing signed package payload: {source}")
                target_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copytree(source_path, target_path)
                packages.append(package)

    if expected_root is None or expected_root.get("public_key") != public_key_from_private():
        raise RuntimeError("local signing key does not match the Registry trust root")
    packages.sort(key=lambda item: (item["identity"], item["version"], item["target"]))
    (site / "root.json").write_text(json.dumps(expected_root, indent=2, sort_keys=True) + "\n")
    index = {
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "key_id": expected_root["key_id"],
        "packages": packages,
        "registry": "hhy-official",
        "schema_version": 1,
    }
    index["signature"] = sign(index)
    (site / "index.json").write_text(json.dumps(index, indent=2, sort_keys=True) + "\n")
    (site / "DEPLOY.txt").write_text(
        "Multi-target HHY Registry. Upload and extract in the BT site root.\n"
    )

    targets = sorted({package["target"] for package in packages})
    archive_output = output / f"hhy-registry-bt-{VERSION}-multi-target.tar.gz"
    with tarfile.open(archive_output, "w:gz") as archive:
        for path in sorted(site.rglob("*")):
            archive.add(path, arcname=path.relative_to(site), recursive=False)
    digest = hashlib.sha256(archive_output.read_bytes()).hexdigest()
    archive_output.with_suffix(archive_output.suffix + ".sha256").write_text(
        f"{digest}  {archive_output.name}\n"
    )
    print(f"Merged targets: {', '.join(targets)}")
    print(f"BT upload archive: {archive_output}")


if __name__ == "__main__":
    main()
