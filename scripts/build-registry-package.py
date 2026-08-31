#!/usr/bin/env python3
"""Build a signed static Registry archive that can be unpacked in a BT site root."""

import base64
import hashlib
import json
import os
import platform
import shutil
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
VERSION = (PROJECT / "VERSION").read_text().strip()
PRIVATE_DIR = PROJECT / ".hhy-private"
PRIVATE_KEY = PRIVATE_DIR / "official-registry-ed25519.pem"
OUTPUT_DIR = PROJECT / "build" / "registry"
SITE_DIR = OUTPUT_DIR / "site"
ARCHIVE = OUTPUT_DIR / f"hhy-registry-bt-{VERSION}.tar.gz"
KEY_ID = "hhy-official-2026-01"


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
    with tempfile.TemporaryDirectory(prefix="hhy-registry-sign-") as temporary:
        payload = Path(temporary) / "payload.json"
        signature = Path(temporary) / "signature.bin"
        payload.write_bytes(canonical(value))
        subprocess.run(
            [OPENSSL, "pkeyutl", "-sign", "-rawin", "-inkey", str(PRIVATE_KEY),
             "-in", str(payload), "-out", str(signature)],
            check=True,
        )
        return base64.b64encode(signature.read_bytes()).decode()


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def file_hashes(directory: Path) -> dict[str, str]:
    return {
        path.relative_to(directory).as_posix(): sha256(path)
        for path in sorted(directory.rglob("*")) if path.is_file()
    }


def copy_package(name: str) -> Path:
    source = PROJECT / "extensions" / name
    target = SITE_DIR / "packages" / name
    target.mkdir(parents=True)
    shutil.copy2(source / "hhy.toml", target / "hhy.toml")
    shutil.copytree(source / "bin", target / "bin")
    library = source / "lib"
    if library.is_dir():
        shutil.copytree(library, target / "lib")
    return target


def ensure_key() -> bytes:
    PRIVATE_DIR.mkdir(mode=0o700, parents=True, exist_ok=True)
    if not PRIVATE_KEY.exists():
        subprocess.run(
            [OPENSSL, "genpkey", "-algorithm", "ED25519", "-out", str(PRIVATE_KEY)],
            check=True,
        )
        PRIVATE_KEY.chmod(0o600)
        print(f"Created private signing key: {PRIVATE_KEY}")
        print("Back it up securely. Never upload this file to BT or GitHub.")
    with tempfile.TemporaryDirectory(prefix="hhy-registry-public-") as temporary:
        public_der = Path(temporary) / "public.der"
        subprocess.run(
            [OPENSSL, "pkey", "-in", str(PRIVATE_KEY), "-pubout", "-outform", "DER",
             "-out", str(public_der)],
            check=True,
        )
        return public_der.read_bytes()[-32:]


def main() -> None:
    public_key = ensure_key()
    if SITE_DIR.exists():
        shutil.rmtree(SITE_DIR)
    SITE_DIR.mkdir(parents=True)

    root = {
        "algorithm": "ed25519",
        "key_id": KEY_ID,
        "public_key": base64.b64encode(public_key).decode(),
        "schema_version": 1,
    }
    (SITE_DIR / "root.json").write_text(json.dumps(root, indent=2, sort_keys=True) + "\n")

    machine = platform.machine().lower()
    architecture = "arm64" if machine in {"arm64", "aarch64"} else machine
    target_name = f"{platform.system().lower()}-{architecture}"
    packages = []
    for name in ("sample", "html", "database"):
        package_dir = copy_package(name)
        item = {
            "dependencies": {},
            "files": file_hashes(package_dir),
            "identity": f"official/{name}",
            "key_id": KEY_ID,
            "runtime_name": name,
            "source": f"packages/{name}",
            "target": target_name,
            "version": "0.1.0" if name != "database" else "0.2.0",
        }
        item["signature"] = sign(item)
        packages.append(item)

    index = {
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "key_id": KEY_ID,
        "packages": packages,
        "registry": "hhy-official",
        "schema_version": 1,
    }
    index["signature"] = sign(index)
    (SITE_DIR / "index.json").write_text(json.dumps(index, indent=2, sort_keys=True) + "\n")
    (SITE_DIR / "DEPLOY.txt").write_text(
        "Upload this archive to /www/wwwroot/registry.hhylang.dev and extract it there.\n"
        "The private signing key is intentionally not included.\n"
    )

    if ARCHIVE.exists():
        ARCHIVE.unlink()
    with tarfile.open(ARCHIVE, "w:gz") as archive:
        for path in sorted(SITE_DIR.rglob("*")):
            archive.add(path, arcname=path.relative_to(SITE_DIR), recursive=False)
    forbidden = {"official-registry-ed25519.pem", "registry-key.pem", "payload.json", "signature.bin"}
    with tarfile.open(ARCHIVE, "r:gz") as archive:
        names = {Path(name).name for name in archive.getnames()}
        if names & forbidden:
            raise RuntimeError("private signing material entered the deployment archive")

    checksum = sha256(ARCHIVE)
    checksum_file = ARCHIVE.with_suffix(ARCHIVE.suffix + ".sha256")
    checksum_file.write_text(f"{checksum}  {ARCHIVE.name}\n")
    print(f"Registry target: {target_name}")
    print(f"BT upload archive: {ARCHIVE}")
    print(f"SHA-256 file: {checksum_file}")


if __name__ == "__main__":
    main()
