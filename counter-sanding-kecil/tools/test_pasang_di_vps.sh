#!/usr/bin/env bash
# Menguji pasang_di_vps.sh dengan rilis NYATA (make_manifest.py + sign_manifest.sh) di folder sementara.
# Jalankan dari folder tools/:  bash test_pasang_di_vps.sh
set -u
cd "$(dirname "$0")" || exit 1
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
lulus=0; gagal=0
ok()   { lulus=$((lulus+1)); echo "LULUS  $1"; }
salah(){ gagal=$((gagal+1)); echo "GAGAL  $1"; }
cek()  { if eval "$2"; then ok "$1"; else salah "$1"; fi; }

openssl genpkey -algorithm ed25519 -out "$T/k.pem" 2>/dev/null

# rilis <build> <folder>: membuat firmware.bin + manifest.signed nyata
rilis() {
  mkdir -p "$2"
  head -c $((20000 + $1 * 7)) /dev/urandom > "$2/firmware.bin"
  python3 make_manifest.py "$2/firmware.bin" --version "fw-v0.0.$1" --build "$1" -o "$2/manifest.json" >/dev/null
  bash sign_manifest.sh "$2/manifest.json" "$T/k.pem" "$2/manifest.signed" >/dev/null
  rm "$2/manifest.json"
}
sha() { sha256sum "$1" | cut -d' ' -f1; }
bld() { tail -c +65 "$1" | sed -n 's/.*"build":\([0-9]*\).*/\1/p'; }

FW="$T/fw"; mkdir -p "$FW"

# 1. rilis pertama
rilis 5 "$T/m5"
bash pasang_di_vps.sh "$T/m5" "$FW" 3 >/dev/null 2>&1; rc=$?
cek "rilis pertama berhasil" '[ $rc -eq 0 ]'
cek "firmware.bin terpasang identik" '[ "$(sha "$FW/firmware.bin")" = "$(sha "$T/m5/firmware.bin")" ]'
cek "manifest.signed terpasang identik" 'cmp -s "$FW/manifest.signed" "$T/m5/manifest.signed"'
cek "arsip build-5 dibuat" '[ -f "$FW/arsip/build-5/firmware.bin" ]'
cek "tidak ada berkas sementara tersisa" '[ -z "$(ls -A "$FW" | grep "^\.")" ]'
cek "berkas dapat dibaca nginx (644)" '[ "$(stat -c %a "$FW/firmware.bin")" = "644" ] && [ "$(stat -c %a "$FW/manifest.signed")" = "644" ]'

# 2. rilis berikutnya mengarsipkan yang lama
rilis 6 "$T/m6"
bash pasang_di_vps.sh "$T/m6" "$FW" 3 >/dev/null 2>&1; rc=$?
cek "rilis 6 berhasil" '[ $rc -eq 0 ] && [ "$(bld "$FW/manifest.signed")" = "6" ]'
cek "arsip memuat build 5 dan 6" '[ -d "$FW/arsip/build-5" ] && [ -d "$FW/arsip/build-6" ]'

# 3. build tidak naik ditolak dan tidak mengubah apa pun
antes_fw="$(sha "$FW/firmware.bin")"; antes_mf="$(sha "$FW/manifest.signed")"
for b in 6 5; do
  rilis $b "$T/ulang$b"
  bash pasang_di_vps.sh "$T/ulang$b" "$FW" 3 >/dev/null 2>&1; rc=$?
  cek "build $b (tidak naik) ditolak" '[ $rc -ne 0 ]'
done
cek "penolakan tidak mengubah berkas terpasang" '[ "$(sha "$FW/firmware.bin")" = "$antes_fw" ] && [ "$(sha "$FW/manifest.signed")" = "$antes_mf" ]'

# 4. hash firmware tidak cocok manifest ditolak
rilis 7 "$T/m7"
printf 'x' >> "$T/m7/firmware.bin"
bash pasang_di_vps.sh "$T/m7" "$FW" 3 >/dev/null 2>&1; rc=$?
cek "firmware diubah setelah ditandatangani ditolak" '[ $rc -ne 0 ]'
cek "folder fw tidak berubah dan arsip build-7 tidak dibuat" '[ "$(sha "$FW/manifest.signed")" = "$antes_mf" ] && [ ! -d "$FW/arsip/build-7" ]'

# 4b. satu byte berubah dengan ukuran TETAP sama: hanya pemeriksaan hash yang bisa menangkapnya
rilis 7 "$T/m7b"
python3 - "$T/m7b/firmware.bin" <<'PY'
import sys
p = sys.argv[1]
d = bytearray(open(p, "rb").read())
d[100] ^= 0x01
open(p, "wb").write(d)
PY
bash pasang_di_vps.sh "$T/m7b" "$FW" 3 >/dev/null 2>&1; rc=$?
cek "satu byte berubah (ukuran sama) ditolak oleh pemeriksaan hash" '[ $rc -ne 0 ]'
cek "berkas terpasang tetap utuh setelah penolakan hash" '[ "$(sha "$FW/manifest.signed")" = "$antes_mf" ] && [ ! -d "$FW/arsip/build-7" ]'

# 5. berkas masuk hilang/kosong ditolak
rilis 8 "$T/m8"; rm "$T/m8/manifest.signed"
bash pasang_di_vps.sh "$T/m8" "$FW" 3 >/dev/null 2>&1; rc=$?
cek "manifest hilang ditolak" '[ $rc -ne 0 ]'
rilis 8 "$T/m8b"; : > "$T/m8b/firmware.bin"
bash pasang_di_vps.sh "$T/m8b" "$FW" 3 >/dev/null 2>&1; rc=$?
cek "firmware kosong ditolak" '[ $rc -ne 0 ]'
bash pasang_di_vps.sh "$T/m8" "$T/tidak-ada" 3 >/dev/null 2>&1; rc=$?
cek "folder fw tidak ada ditolak" '[ $rc -ne 0 ]'

# 6. pemangkasan arsip: simpan 3 build terbesar
for b in 9 10 11; do rilis $b "$T/m$b"; bash pasang_di_vps.sh "$T/m$b" "$FW" 3 >/dev/null 2>&1; done
cek "arsip dipangkas ke 3 build terbesar (9,10,11)" '[ "$(ls "$FW/arsip" | sort -V | tr "\n" " ")" = "build-9 build-10 build-11 " ]'
cek "build terlama (5,6) terhapus, terbaru tetap" '[ ! -d "$FW/arsip/build-5" ] && [ ! -d "$FW/arsip/build-6" ] && [ -d "$FW/arsip/build-11" ] && [ -d "$FW/arsip/build-9" ]'
cek "pemangkasan numerik (10 > 9), bukan abjad" 'rilis 12 "$T/m12"; bash pasang_di_vps.sh "$T/m12" "$FW" 3 >/dev/null 2>&1; [ ! -d "$FW/arsip/build-9" ] && [ -d "$FW/arsip/build-10" ]'

# 7. argumen salah
bash pasang_di_vps.sh >/dev/null 2>&1; cek "tanpa argumen ditolak" '[ $? -ne 0 ]'
bash pasang_di_vps.sh "$T/m12" "$FW" 1 >/dev/null 2>&1; cek "jumlah_arsip < 2 ditolak" '[ $? -ne 0 ]'
bash pasang_di_vps.sh "$T/m12" "$FW" abc >/dev/null 2>&1; cek "jumlah_arsip bukan angka ditolak" '[ $? -ne 0 ]'

echo; echo "$lulus lulus, $gagal gagal"
[ "$gagal" -eq 0 ]