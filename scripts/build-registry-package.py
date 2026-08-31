#!/usr/bin/env python3
"""Build a signed static Registry archive that can be unpacked in a BT site root."""

import base64
import argparse
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


def package_version(manifest: Path) -> str:
    for line in manifest.read_text().splitlines():
        if line.strip().startswith("version = "):
            return line.split('"', 2)[1]
    raise RuntimeError(f"package version is missing from {manifest}")


def copy_package(source_root: Path, site_dir: Path, name: str, version: str, target_name: str) -> Path:
    source = source_root / name
    target = site_dir / "packages" / "official" / name / version / target_name
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
    parser = argparse.ArgumentParser()
    parser.add_argument("--extensions-dir", type=Path, default=PROJECT / "extensions")
    parser.add_argument("--target")
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR)
    args = parser.parse_args()
    public_key = ensure_key()
    source_root = args.extensions_dir.resolve()

    root = {
        "algorithm": "ed25519",
        "key_id": KEY_ID,
        "public_key": base64.b64encode(public_key).decode(),
        "schema_version": 1,
    }
    machine = platform.machine().lower()
    architecture = "arm64" if machine in {"arm64", "aarch64"} else machine
    detected_target = f"{'darwin' if platform.system().lower() == 'darwin' else platform.system().lower()}-{architecture}"
    target_name = args.target or detected_target
    if args.target and args.target != detected_target and args.extensions_dir == PROJECT / "extensions":
        raise RuntimeError("--target override requires --extensions-dir with binaries built on that target")
    output_dir = args.output_dir.resolve()
    site_dir = output_dir / f"site-{target_name}"
    if site_dir.exists():
        shutil.rmtree(site_dir)
    site_dir.mkdir(parents=True)
    (site_dir / "root.json").write_text(json.dumps(root, indent=2, sort_keys=True) + "\n")

    packages = []
    for name in ("sample", "html", "database"):
        source_package = source_root / name
        if not (source_package / "hhy.toml").is_file() or not (source_package / "bin").is_dir():
            continue
        version = package_version(source_package / "hhy.toml")
        package_dir = copy_package(source_root, site_dir, name, version, target_name)
        item = {
            "dependencies": {},
            "files": file_hashes(package_dir),
            "identity": f"official/{name}",
            "key_id": KEY_ID,
            "runtime_name": name,
            "source": f"packages/official/{name}/{version}/{target_name}",
            "target": target_name,
            "version": version,
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
    if not packages:
        raise RuntimeError(f"no built extensions found under {source_root}")
    (site_dir / "index.json").write_text(json.dumps(index, indent=2, sort_keys=True) + "\n")
    (site_dir / "DEPLOY.txt").write_text(
        "Upload this archive to /www/wwwroot/registry.hhylang.dev and extract it there.\n"
        "The private signing key is intentionally not included.\n"
    )

    archive_path = output_dir / f"hhy-registry-{target_name}-{VERSION}.tar.gz"
    if archive_path.exists():
        archive_path.unlink()
    with tarfile.open(archive_path, "w:gz") as archive:
        for path in sorted(site_dir.rglob("*")):
            archive.add(path, arcname=path.relative_to(site_dir), recursive=False)
    forbidden = {"official-registry-ed25519.pem", "registry-key.pem", "payload.json", "signature.bin"}
    with tarfile.open(archive_path, "r:gz") as archive:
        names = {Path(name).name for name in archive.getnames()}
        if names & forbidden:
            raise RuntimeError("private signing material entered the deployment archive")

    checksum = sha256(archive_path)
    checksum_file = archive_path.with_suffix(archive_path.suffix + ".sha256")
    checksum_file.write_text(f"{checksum}  {archive_path.name}\n")
    print(f"Registry target: {target_name}")
    print(f"Target archive: {archive_path}")
    print(f"SHA-256 file: {checksum_file}")


if __name__ == "__main__":
    main()
