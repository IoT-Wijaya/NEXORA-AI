#!/usr/bin/env bash
# Menandatangani manifest.json -> manifest.signed ([64 byte tanda tangan][JSON]).
# Pemakaian: tools/sign_manifest.sh manifest.json kunci_rahasia.pem manifest.signed
# Di CI, kunci rahasia ditulis dari GitHub Secret ke berkas sementara lalu dihapus.
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "pemakaian: $0 manifest.json kunci_rahasia.pem manifest.signed" >&2
    exit 2
fi
MANIFEST="$1"; KEY="$2"; OUT="$3"

[ -s "$MANIFEST" ] || { echo "GAGAL: manifest kosong atau tidak ada" >&2; exit 1; }
[ "$(wc -c < "$MANIFEST")" -le 1024 ] || { echo "GAGAL: manifest > 1024 byte" >&2; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

openssl pkeyutl -sign -rawin -inkey "$KEY" -in "$MANIFEST" -out "$WORK/sig"
[ "$(wc -c < "$WORK/sig")" -eq 64 ] || { echo "GAGAL: tanda tangan bukan 64 byte (kunci bukan Ed25519?)" >&2; exit 1; }

# Periksa sendiri sebelum diserahkan: tanda tangan harus sah untuk kunci publik pasangannya.
openssl pkey -in "$KEY" -pubout -out "$WORK/pub.pem"
openssl pkeyutl -verify -rawin -pubin -inkey "$WORK/pub.pem" -in "$MANIFEST" -sigfile "$WORK/sig" >/dev/null

cat "$WORK/sig" "$MANIFEST" > "$OUT"
echo "ditulis: $OUT ($(wc -c < "$OUT") byte)"