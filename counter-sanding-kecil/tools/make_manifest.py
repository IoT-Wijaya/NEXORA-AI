#!/usr/bin/env python3
"""Buat manifest.json untuk satu rilis firmware.

Pemakaian:
    python tools/make_manifest.py .pio/build/sanding_kecil/firmware.bin \
        --version fw-v1.0.0 --build 12 -o manifest.json

Aturan kolom SAMA dengan ManifestParser di firmware (lib/ota_core/manifest.cpp).
Skrip menolak lebih awal apa yang pasti ditolak perangkat, supaya kesalahan
ketahuan di CI, bukan di lapangan.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

SCHEMA = 1
MAX_MANIFEST_BYTES = 1024            # ManifestLimits::MAX_BYTES
MAX_FIRMWARE_BYTES = 16 * 1024 * 1024  # ManifestLimits::MAX_FIRMWARE_BYTES
DEFAULT_SLOT_BYTES = 0x140000        # slot OTA default.csv (1,25 MB)

TYPE_RE = re.compile(r"[a-z0-9-]{1,23}")
VERSION_RE = re.compile(r"[A-Za-z0-9._+-]{1,63}")


class ManifestError(ValueError):
    pass


def build_manifest(firmware: bytes, *, version: str, build: int,
                   device_type: str = "sanding-kecil", board: str = "nodemcu-32s",
                   min_from_build: int = 0, allow_downgrade: bool = False,
                   max_size: int = DEFAULT_SLOT_BYTES) -> bytes:
    """Kembalikan byte JSON manifest. Melempar ManifestError bila ada yang salah."""
    if not TYPE_RE.fullmatch(device_type):
        raise ManifestError("device_type harus a-z, 0-9, '-' dan 1-23 karakter")
    if not TYPE_RE.fullmatch(board):
        raise ManifestError("board harus a-z, 0-9, '-' dan 1-23 karakter")
    if not VERSION_RE.fullmatch(version):
        raise ManifestError("version harus A-Z a-z 0-9 . _ + - dan 1-63 karakter")
    if not 1 <= build <= 0xFFFFFFFF:
        raise ManifestError("build harus 1 sampai 4294967295")
    if not 0 <= min_from_build <= 0xFFFFFFFF:
        raise ManifestError("min_from_build di luar rentang")
    if min_from_build >= build:
        raise ManifestError("min_from_build harus lebih kecil dari build rilis ini")
    size = len(firmware)
    if size < 1:
        raise ManifestError("firmware kosong")
    if size > MAX_FIRMWARE_BYTES:
        raise ManifestError("firmware melebihi batas kewarasan 16 MB")
    if size > max_size:
        raise ManifestError(
            f"firmware {size} byte melebihi slot {max_size} byte; perangkat akan menolaknya")

    doc = {
        "schema": SCHEMA,
        "device_type": device_type,
        "board": board,
        "version": version,
        "build": build,
        "size": size,
        "sha256": hashlib.sha256(firmware).hexdigest(),
    }
    if min_from_build:
        doc["min_from_build"] = min_from_build
    if allow_downgrade:
        doc["allow_downgrade"] = True

    out = json.dumps(doc, separators=(",", ":")).encode("ascii")
    if len(out) > MAX_MANIFEST_BYTES:
        raise ManifestError("manifest melebihi 1024 byte")
    return out


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="Buat manifest.json untuk firmware.")
    p.add_argument("firmware", type=Path, help="berkas firmware.bin")
    p.add_argument("--version", required=True)
    p.add_argument("--build", required=True, type=int)
    p.add_argument("--device-type", default="sanding-kecil")
    p.add_argument("--board", default="nodemcu-32s")
    p.add_argument("--min-from-build", type=int, default=0)
    p.add_argument("--allow-downgrade", action="store_true")
    p.add_argument("--max-size", type=int, default=DEFAULT_SLOT_BYTES)
    p.add_argument("-o", "--output", type=Path, default=Path("manifest.json"))
    a = p.parse_args(argv)

    try:
        data = a.firmware.read_bytes()
        manifest = build_manifest(
            data, version=a.version, build=a.build, device_type=a.device_type,
            board=a.board, min_from_build=a.min_from_build,
            allow_downgrade=a.allow_downgrade, max_size=a.max_size)
    except (OSError, ManifestError) as e:
        print("GAGAL:", e, file=sys.stderr)
        return 1

    a.output.write_bytes(manifest)  # tanpa baris baru: tanda tangan atas byte persis ini
    print(f"ditulis: {a.output} ({len(manifest)} byte), firmware {len(data)} byte")
    return 0


if __name__ == "__main__":
    sys.exit(main())