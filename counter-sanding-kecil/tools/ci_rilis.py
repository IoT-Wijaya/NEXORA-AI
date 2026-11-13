#!/usr/bin/env python3
"""Pembantu CI untuk rilis firmware. Logikanya di sini (bukan di YAML) supaya bisa diuji.

Subperintah:
  versi     --tag sanding-kecil-v1.2.0 --commits 57
            -> mencetak FW_VERSION=fw-v1.2.0 dan FW_BUILD=57 (untuk $GITHUB_ENV)
  cek-naik  --baru 57 [--terpasang manifest.signed]
            -> gagal bila nomor build tidak lebih besar dari rilis yang sudah terpasang di server
"""
import argparse
import json
import re
import sys
from pathlib import Path

TAG_RE = re.compile(r"sanding-kecil-v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")
SIGNATURE_BYTES = 64
MAX_BUILD = 0xFFFFFFFF


class RilisError(ValueError):
    pass


def versi_dari_tag(tag: str) -> str:
    """'sanding-kecil-v1.2.0' -> 'fw-v1.2.0'. Menolak bentuk lain, termasuk akhiran apa pun."""
    m = TAG_RE.fullmatch(tag)
    if not m:
        raise RilisError(f"tag '{tag}' tidak sesuai pola sanding-kecil-vX.Y.Z")
    return "fw-v{}.{}.{}".format(*m.groups())


def build_dari_jumlah_commit(jumlah: int) -> int:
    """Nomor build = jumlah commit di riwayat tag. Naik setiap commit, sama untuk commit yang sama."""
    if not 1 <= jumlah <= MAX_BUILD:
        raise RilisError("jumlah commit di luar rentang build 1..4294967295")
    return jumlah


def baca_build_manifest(signed: bytes) -> int:
    """Membaca nomor build dari manifest.signed. Tanda tangan TIDAK diverifikasi di sini:
    ini hanya pemeriksaan kewarasan terhadap berkas milik kita sendiri di server."""
    if len(signed) <= SIGNATURE_BYTES:
        raise RilisError("manifest terpasang terlalu pendek")
    try:
        data = json.loads(signed[SIGNATURE_BYTES:].decode("ascii"))
    except (UnicodeDecodeError, ValueError) as e:
        raise RilisError(f"manifest terpasang tidak terbaca: {e}")
    build = data.get("build") if isinstance(data, dict) else None
    if not isinstance(build, int) or isinstance(build, bool) or not 1 <= build <= MAX_BUILD:
        raise RilisError("manifest terpasang tidak memuat build yang sah")
    return build


def cek_naik(baru: int, terpasang: "int | None") -> None:
    if terpasang is not None and baru <= terpasang:
        raise RilisError(
            f"build {baru} tidak lebih besar dari build terpasang {terpasang}; "
            "rilis dibatalkan agar perangkat tidak menolak update"
        )


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("versi")
    v.add_argument("--tag", required=True)
    v.add_argument("--commits", required=True, type=int)
    c = sub.add_parser("cek-naik")
    c.add_argument("--baru", required=True, type=int)
    c.add_argument("--terpasang", type=Path, help="manifest.signed yang sedang terpasang; tidak ada = rilis pertama")
    a = p.parse_args(argv)

    try:
        if a.cmd == "versi":
            print("FW_VERSION=" + versi_dari_tag(a.tag))
            print("FW_BUILD=" + str(build_dari_jumlah_commit(a.commits)))
        else:
            terpasang = None
            if a.terpasang is not None and a.terpasang.exists():
                terpasang = baca_build_manifest(a.terpasang.read_bytes())
            cek_naik(a.baru, terpasang)
            print("OK: build {} {}".format(a.baru, "rilis pertama" if terpasang is None else f"> terpasang {terpasang}"))
    except RilisError as e:
        print("GAGAL:", e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())