#!/usr/bin/env python3
"""Pembantu CI untuk rilis firmware. Logikanya di sini (bukan di YAML) supaya bisa diuji.

Subperintah:
  versi     --tag sanding-besar-v1.2.0 --commits 57
            -> mencetak FW_MESIN, FW_VERSION, FW_BUILD, FW_DEVICE_TYPE, FW_BOARD, PIO_ENV (untuk $GITHUB_ENV)
  cek-naik  --baru 57 [--terpasang manifest.signed] [--device-type sanding-besar]
            -> gagal bila build tidak lebih besar dari rilis terpasang, atau manifest terpasang
               milik jenis perangkat lain (salah folder)

Menambah mesin baru: tambahkan satu baris di MESIN, buat env PlatformIO dan berkas
include/<env>_config.h; uji di test_ci_rilis.py memeriksa ketiganya tetap sinkron.
"""
import argparse
import json
import re
import sys
from pathlib import Path
from typing import NamedTuple, Optional


class Mesin(NamedTuple):
    pio_env: str       # env di platformio.ini
    device_type: str   # harus sama dengan OTA_DEVICE_TYPE di firmware dan device_type di manifest
    board: str


# Kunci = slug = awalan tag = nama folder rilis di server (/srv/firmware/<slug>/).
MESIN = {
    "sanding-kecil": Mesin("sanding_kecil", "sanding-kecil", "nodemcu-32s"),
    "sanding-besar": Mesin("sanding_besar", "sanding-besar", "nodemcu-32s"),
}

TAG_RE = re.compile(
    "(?P<mesin>" + "|".join(re.escape(k) for k in MESIN) + r")-v"
    r"(?P<a>0|[1-9][0-9]*)\.(?P<b>0|[1-9][0-9]*)\.(?P<c>0|[1-9][0-9]*)"
)
SIGNATURE_BYTES = 64
MAX_BUILD = 0xFFFFFFFF


class RilisError(ValueError):
    pass


def mesin_dari_tag(tag: str) -> "tuple[str, str]":
    """'sanding-besar-v1.2.0' -> ('sanding-besar', 'fw-v1.2.0'). Menolak bentuk lain, termasuk akhiran apa pun."""
    m = TAG_RE.fullmatch(tag)
    if not m:
        raise RilisError(f"tag '{tag}' tidak sesuai pola <mesin>-vX.Y.Z dengan mesin salah satu dari: "
                         + ", ".join(MESIN))
    return m["mesin"], "fw-v{}.{}.{}".format(m["a"], m["b"], m["c"])


def versi_dari_tag(tag: str) -> str:
    return mesin_dari_tag(tag)[1]


def build_dari_jumlah_commit(jumlah: int) -> int:
    """Nomor build = jumlah commit di riwayat tag. Naik setiap commit, sama untuk commit yang sama."""
    if not 1 <= jumlah <= MAX_BUILD:
        raise RilisError("jumlah commit di luar rentang build 1..4294967295")
    return jumlah


def _isi_manifest(signed: bytes) -> dict:
    if len(signed) <= SIGNATURE_BYTES:
        raise RilisError("manifest terpasang terlalu pendek")
    try:
        data = json.loads(signed[SIGNATURE_BYTES:].decode("ascii"))
    except (UnicodeDecodeError, ValueError) as e:
        raise RilisError(f"manifest terpasang tidak terbaca: {e}")
    if not isinstance(data, dict):
        raise RilisError("manifest terpasang bukan objek JSON")
    return data


def baca_build_manifest(signed: bytes) -> int:
    """Membaca nomor build dari manifest.signed. Tanda tangan TIDAK diverifikasi di sini:
    ini hanya pemeriksaan kewarasan terhadap berkas milik kita sendiri di server."""
    build = _isi_manifest(signed).get("build")
    if not isinstance(build, int) or isinstance(build, bool) or not 1 <= build <= MAX_BUILD:
        raise RilisError("manifest terpasang tidak memuat build yang sah")
    return build


def baca_device_type_manifest(signed: bytes) -> "str | None":
    dt = _isi_manifest(signed).get("device_type")
    return dt if isinstance(dt, str) else None


def cek_naik(baru: int, terpasang: "int | None") -> None:
    if terpasang is not None and baru <= terpasang:
        raise RilisError(
            f"build {baru} tidak lebih besar dari build terpasang {terpasang}; "
            "rilis dibatalkan agar perangkat tidak menolak update"
        )


def cek_jenis_sama(terpasang_device_type: "str | None", diharapkan: str) -> None:
    """Folder mesin A tidak boleh berisi manifest mesin B (tanda salah folder atau salah tag)."""
    if terpasang_device_type is not None and terpasang_device_type != diharapkan:
        raise RilisError(
            f"manifest terpasang milik '{terpasang_device_type}', bukan '{diharapkan}'; "
            "periksa folder rilis dan tag"
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
    c.add_argument("--device-type", help="jenis perangkat yang diharapkan pada manifest terpasang")
    a = p.parse_args(argv)

    try:
        if a.cmd == "versi":
            slug, versi = mesin_dari_tag(a.tag)
            m = MESIN[slug]
            print("FW_MESIN=" + slug)
            print("FW_VERSION=" + versi)
            print("FW_BUILD=" + str(build_dari_jumlah_commit(a.commits)))
            print("FW_DEVICE_TYPE=" + m.device_type)
            print("FW_BOARD=" + m.board)
            print("PIO_ENV=" + m.pio_env)
        else:
            terpasang = None
            if a.terpasang is not None and a.terpasang.exists():
                data = a.terpasang.read_bytes()
                terpasang = baca_build_manifest(data)
                if a.device_type:
                    cek_jenis_sama(baca_device_type_manifest(data), a.device_type)
            cek_naik(a.baru, terpasang)
            print("OK: build {} {}".format(a.baru, "rilis pertama" if terpasang is None else f"> terpasang {terpasang}"))
    except RilisError as e:
        print("GAGAL:", e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())