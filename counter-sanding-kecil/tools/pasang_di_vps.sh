#!/usr/bin/env bash
# Dijalankan DI VPS (lewat ssh dari CI): memasang rilis baru ke folder /fw/ yang disajikan nginx.
#   pasang_di_vps.sh <folder_masuk> <folder_fw> [jumlah_arsip=10]
#
# <folder_masuk> berisi firmware.bin dan manifest.signed hasil CI. Skrip ini:
#   1. memeriksa hash firmware terhadap manifest dan nomor build terhadap rilis terpasang,
#   2. mengarsipkan rilis lama dan baru di <folder_fw>/arsip/build-<n>/,
#   3. memasang firmware.bin LEBIH DULU, manifest.signed TERAKHIR (masing-masing dengan mv
#      atomik), karena perangkat memulai dari manifest,
#   4. memangkas arsip lama.
# Gagal di langkah mana pun sebelum langkah 3 tidak mengubah apa pun di <folder_fw>.
set -euo pipefail

[ "$#" -ge 2 ] || { echo "pemakaian: $0 folder_masuk folder_fw [jumlah_arsip]" >&2; exit 2; }
MASUK="$1"; FW="$2"; SIMPAN="${3:-10}"
case "$SIMPAN" in ''|*[!0-9]*) echo "GAGAL: jumlah_arsip harus angka" >&2; exit 2;; esac
[ "$SIMPAN" -ge 2 ] || { echo "GAGAL: jumlah_arsip minimal 2" >&2; exit 2; }

gagal() { echo "GAGAL: $*" >&2; exit 1; }

[ -s "$MASUK/firmware.bin" ] || gagal "firmware.bin tidak ada atau kosong di $MASUK"
[ -s "$MASUK/manifest.signed" ] || gagal "manifest.signed tidak ada atau kosong di $MASUK"
[ -d "$FW" ] || gagal "folder $FW tidak ada"

# isi manifest = semua byte setelah 64 byte tanda tangan
json_dari() { tail -c +65 "$1"; }
kolom() { json_dari "$1" | sed -n "s/.*\"$2\":\\([0-9]\\{1,10\\}\\)[,}].*/\\1/p"; }
sha_dari() { json_dari "$1" | sed -n 's/.*"sha256":"\([0-9a-f]\{64\}\)".*/\1/p'; }

BARU_BUILD="$(kolom "$MASUK/manifest.signed" build)"
case "$BARU_BUILD" in ''|*[!0-9]*) gagal "build tidak terbaca dari manifest";; esac
SHA_MANIFEST="$(sha_dari "$MASUK/manifest.signed")"
[ -n "$SHA_MANIFEST" ] || gagal "sha256 tidak terbaca dari manifest"
SHA_BERKAS="$(sha256sum "$MASUK/firmware.bin" | cut -d' ' -f1)"
[ "$SHA_MANIFEST" = "$SHA_BERKAS" ] || gagal "hash firmware.bin tidak cocok dengan manifest"
UKURAN_MANIFEST="$(kolom "$MASUK/manifest.signed" size)"
[ "$UKURAN_MANIFEST" = "$(wc -c < "$MASUK/firmware.bin" | tr -d ' ')" ] || gagal "ukuran firmware.bin tidak cocok dengan manifest"

LAMA_BUILD=""
if [ -f "$FW/manifest.signed" ]; then
  LAMA_BUILD="$(kolom "$FW/manifest.signed" build)"
  case "$LAMA_BUILD" in ''|*[!0-9]*) gagal "manifest terpasang tidak terbaca";; esac
  [ "$BARU_BUILD" -gt "$LAMA_BUILD" ] || gagal "build $BARU_BUILD tidak lebih besar dari yang terpasang ($LAMA_BUILD)"
fi

ARSIP="$FW/arsip"
mkdir -p "$ARSIP"
if [ -n "$LAMA_BUILD" ] && [ -f "$FW/firmware.bin" ] && [ ! -d "$ARSIP/build-$LAMA_BUILD" ]; then
  mkdir "$ARSIP/build-$LAMA_BUILD"
  cp "$FW/firmware.bin" "$FW/manifest.signed" "$ARSIP/build-$LAMA_BUILD/"
fi
mkdir -p "$ARSIP/build-$BARU_BUILD"
cp "$MASUK/firmware.bin" "$MASUK/manifest.signed" "$ARSIP/build-$BARU_BUILD/"

# Siapkan di folder yang sama agar mv atomik (satu sistem berkas), lalu pasang berurutan.
cp "$MASUK/firmware.bin" "$FW/.firmware.bin.baru"
cp "$MASUK/manifest.signed" "$FW/.manifest.signed.baru"
chmod 644 "$FW/.firmware.bin.baru" "$FW/.manifest.signed.baru"
sync
mv -f "$FW/.firmware.bin.baru" "$FW/firmware.bin"
mv -f "$FW/.manifest.signed.baru" "$FW/manifest.signed"

# Pangkas arsip: pertahankan $SIMPAN build terbesar.
ls -1 "$ARSIP" | sed -n 's/^build-\([0-9]\{1,10\}\)$/\1/p' | sort -rn | tail -n +"$((SIMPAN + 1))" | while read -r n; do
  rm -rf "${ARSIP:?}/build-$n"
done

echo "terpasang: build $BARU_BUILD (sebelumnya ${LAMA_BUILD:-tidak ada}), sha256 $SHA_BERKAS"