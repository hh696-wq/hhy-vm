#!/usr/bin/env python3
"""Build an ephemeral, signed static Registry from the compiled test extensions."""

import base64
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def openssl_command() -> str:
    try:
        prefix = subprocess.run(
            ["brew", "--prefix", "openssl@3"], check=True, capture_output=True, text=True
        ).stdout.strip()
        candidate = Path(prefix) / "bin" / "openssl"
        if candidate.is_file():
            return str(candidate)
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    return "openssl"


OPENSSL = openssl_command()


def runtime_target() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    architecture = "arm64" if machine in {"arm64", "aarch64"} else "x86_64" if machine in {"amd64", "x86_64"} else machine
    return f"{'darwin' if system == 'darwin' else 'windows' if system.startswith('win') else system}-{architecture}"


def canonical(value: dict) -> bytes:
    return json.dumps(value, ensure_ascii=True, separators=(",", ":"), sort_keys=True).encode()


def sign(value: dict, private_key: Path) -> str:
    payload = private_key.parent / "payload.json"
    signature = private_key.parent / "signature.bin"
    payload.write_bytes(canonical(value))
    subprocess.run(
        [OPENSSL, "pkeyutl", "-sign", "-rawin", "-inkey", str(private_key),
         "-in", str(payload), "-out", str(signature)],
        check=True,
    )
    return base64.b64encode(signature.read_bytes()).decode()


def file_hashes(directory: Path) -> dict[str, str]:
    return {
        str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(directory.rglob("*")) if path.is_file()
    }


def main() -> None:
    target = Path(sys.argv[1]).resolve()
    project = Path(__file__).resolve().parents[1]
    target.mkdir(parents=True, exist_ok=True)
    private_key = target / "registry-key.pem"
    public_der = target / "registry-public.der"
    subprocess.run([OPENSSL, "genpkey", "-algorithm", "ED25519", "-out", str(private_key)], check=True)
    subprocess.run(
        [OPENSSL, "pkey", "-in", str(private_key), "-pubout", "-outform", "DER", "-out", str(public_der)],
        check=True,
    )
    raw_public_key = public_der.read_bytes()[-32:]
    root = {"algorithm": "ed25519", "key_id": "hhy-test-2026", "public_key": base64.b64encode(raw_public_key).decode(), "schema_version": 1}
    (target / "root.json").write_text(json.dumps(root, indent=2, sort_keys=True) + "\n")

    configured_targets = os.environ.get(
        "HHY_REGISTRY_FIXTURE_TARGETS",
        "darwin-arm64,linux-x86_64,linux-arm64,windows-x86_64",
    ).split(",")
    packages = []
    for package_target in configured_targets:
        for name, dependencies in (("html", {}), ("sample", {"official/html": "^0.1.0"})):
            package_dir = target / "packages" / "official" / name / "0.1.0" / package_target
            if not package_dir.exists():
                shutil.copytree(project / "extensions" / name, package_dir)
            item = {
                "dependencies": dependencies,
                "files": file_hashes(package_dir),
                "identity": f"official/{name}",
                "key_id": "hhy-test-2026",
                "runtime_name": name,
                "source": f"packages/official/{name}/0.1.0/{package_target}",
                "target": package_target,
                "version": "0.1.0",
            }
            item["signature"] = sign(item, private_key)
            packages.append(item)
    index = {"generated_at": "2026-08-31T00:00:00Z", "key_id": "hhy-test-2026", "packages": packages, "registry": "hhy-test", "schema_version": 1}
    index["signature"] = sign(index, private_key)
    (target / "index.json").write_text(json.dumps(index, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
