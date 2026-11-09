#!/usr/bin/env python3
"""Ubah kunci PUBLIK Ed25519 (PEM) menjadi include/ota_public_key.h.

Pemakaian:
    python tools/make_public_key.py ota_public.pem [include/ota_public_key.h]

Hanya butuh Python; tidak perlu OpenSSL di langkah ini. Berkas masukan
HARUS kunci publik ("BEGIN PUBLIC KEY"). Kunci rahasia ditolak.
"""
import base64
import sys
from pathlib import Path

# Awalan DER SubjectPublicKeyInfo untuk Ed25519; sisanya 32 byte kunci.
SPKI_PREFIX = bytes.fromhex("302a300506032b6570032100")
KEY_LEN = 32


def load_public_key(pem_text: str) -> bytes:
    if "PRIVATE KEY" in pem_text:
        raise ValueError("ini kunci RAHASIA; berikan kunci publik saja")
    lines = [l.strip() for l in pem_text.strip().splitlines()]
    if not lines or lines[0] != "-----BEGIN PUBLIC KEY-----" or lines[-1] != "-----END PUBLIC KEY-----":
        raise ValueError("bukan PEM kunci publik")
    der = base64.b64decode("".join(lines[1:-1]), validate=True)
    if len(der) != len(SPKI_PREFIX) + KEY_LEN or not der.startswith(SPKI_PREFIX):
        raise ValueError("bukan kunci publik Ed25519")
    key = der[len(SPKI_PREFIX):]
    if not any(key):
        raise ValueError("kunci publik berisi nol semua")
    return key


def render_header(key: bytes) -> str:
    rows = [key[i:i + 8] for i in range(0, KEY_LEN, 8)]
    body = ",\n".join("    " + ", ".join("0x%02x" % b for b in r) for r in rows)
    return (
        "#pragma once\n"
        "#include <stdint.h>\n\n"
        "// DIHASILKAN oleh tools/make_public_key.py. Jangan disunting manual.\n"
        "// Kunci PUBLIK penanda tangan rilis. Aman di-commit; kunci rahasianya\n"
        "// hanya ada di GitHub Secrets.\n"
        "namespace ota {\n\n"
        "constexpr uint8_t SIGNING_PUBLIC_KEY[32] = {\n" + body + "\n};\n\n"
        "}  // namespace ota\n"
    )


def main(argv) -> int:
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    try:
        key = load_public_key(Path(argv[1]).read_text())
    except (OSError, ValueError) as e:
        print("GAGAL:", e, file=sys.stderr)
        return 1
    out = Path(argv[2] if len(argv) == 3 else "include/ota_public_key.h")
    out.write_text(render_header(key))
    print("ditulis:", out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))