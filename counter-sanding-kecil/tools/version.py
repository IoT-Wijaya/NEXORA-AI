Import("env")
import os
import re
import subprocess

MAKS_PANJANG = 63


def git(*args):
    try:
        return subprocess.check_output(["git"] + list(args), stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        return ""


# Nilai dari CI (variabel lingkungan) menang atas hasil git.
versi = os.environ.get("FW_VERSION") or git("describe", "--tags", "--always", "--dirty") or "unknown"
versi = re.sub(r"[^A-Za-z0-9._+-]", "_", versi)[:MAKS_PANJANG]

build = os.environ.get("FW_BUILD", "0")
if not build.isdigit():
    build = "0"

isi = (
    "#pragma once\n"
    "// Dihasilkan otomatis oleh tools/version.py. JANGAN diedit atau di-commit.\n"
    '#define FW_VERSION "%s"\n'
    "#define FW_BUILD %sUL\n"
) % (versi, build)

jalur = os.path.join(env["PROJECT_INCLUDE_DIR"], "build_info.h")
lama = ""
if os.path.exists(jalur):
    with open(jalur) as f:
        lama = f.read()

# Hanya menulis jika isinya berubah, supaya waktu ubah berkas tidak memicu kompilasi ulang sia-sia.
if lama != isi:
    with open(jalur, "w") as f:
        f.write(isi)