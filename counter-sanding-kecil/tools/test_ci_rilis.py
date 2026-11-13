import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

import ci_rilis
from ci_rilis import RilisError, baca_build_manifest, build_dari_jumlah_commit, cek_naik, versi_dari_tag


def signed(build, extra=None):
    d = {"schema": 1, "build": build}
    d.update(extra or {})
    return bytes(64) + json.dumps(d).encode()


def jalankan(*argv):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        rc = ci_rilis.main(list(argv))
    return rc, out.getvalue(), err.getvalue()


class VersiDariTag(unittest.TestCase):
    def test_tag_sah(self):
        self.assertEqual(versi_dari_tag("sanding-kecil-v1.2.0"), "fw-v1.2.0")
        self.assertEqual(versi_dari_tag("sanding-kecil-v0.0.1"), "fw-v0.0.1")
        self.assertEqual(versi_dari_tag("sanding-kecil-v10.20.300"), "fw-v10.20.300")

    def test_tag_salah_ditolak(self):
        for t in ["", "v1.2.0", "sanding-kecil-v1.2", "sanding-kecil-v1.2.0-rc1", "sanding-kecil-v1.2.0\n",
                  "sanding-kecil-v01.2.0", "sanding-kecil-v1.2.x", "Sanding-Kecil-v1.2.0", "xsanding-kecil-v1.2.0",
                  "sanding-kecil-v1.2.0 ", "refs/tags/sanding-kecil-v1.2.0", "hmi-sanding-v1.2.0"]:
            with self.assertRaises(RilisError, msg=repr(t)):
                versi_dari_tag(t)

    def test_hasil_selalu_lolos_aturan_versi_manifest(self):
        import re
        pola = re.compile(r"[A-Za-z0-9._+-]{1,63}")  # VERSION_RE di make_manifest.py
        self.assertIsNotNone(pola.fullmatch(versi_dari_tag("sanding-kecil-v123.456.789")))


class BuildDariCommit(unittest.TestCase):
    def test_rentang(self):
        self.assertEqual(build_dari_jumlah_commit(1), 1)
        self.assertEqual(build_dari_jumlah_commit(0xFFFFFFFF), 0xFFFFFFFF)
        for n in (0, -1, 0x100000000):
            with self.assertRaises(RilisError):
                build_dari_jumlah_commit(n)


class BacaBuildManifest(unittest.TestCase):
    def test_sah(self):
        self.assertEqual(baca_build_manifest(signed(57)), 57)

    def test_rusak_ditolak(self):
        buruk = [b"", bytes(64), bytes(64) + b"bukan json", bytes(64) + b"[1]", signed(0), signed(-1),
                 signed(0x100000000), signed("5"), signed(True), signed(5.5), bytes(64) + b'{"x":1}',
                 bytes(64) + "{\"build\":5}".encode("utf-16")]
        for b in buruk:
            with self.assertRaises(RilisError, msg=repr(b[:80])):
                baca_build_manifest(b)


class CekNaik(unittest.TestCase):
    def test_lebih_besar_lolos_dan_pertama_lolos(self):
        cek_naik(58, 57)
        cek_naik(1, None)

    def test_sama_atau_lebih_kecil_ditolak(self):
        for baru, lama in ((57, 57), (56, 57), (1, 2)):
            with self.assertRaises(RilisError):
                cek_naik(baru, lama)


class AntarmukaBarisPerintah(unittest.TestCase):
    def test_versi_mencetak_dua_baris_siap_github_env(self):
        rc, out, _ = jalankan("versi", "--tag", "sanding-kecil-v1.2.0", "--commits", "57")
        self.assertEqual(rc, 0)
        self.assertEqual(out.splitlines(), ["FW_VERSION=fw-v1.2.0", "FW_BUILD=57"])

    def test_versi_tag_salah_gagal_tanpa_keluaran(self):
        rc, out, err = jalankan("versi", "--tag", "v1", "--commits", "5")
        self.assertEqual(rc, 1)
        self.assertEqual(out, "")
        self.assertIn("GAGAL", err)

    def test_cek_naik_tanpa_manifest_terpasang_adalah_rilis_pertama(self):
        with tempfile.TemporaryDirectory() as d:
            rc, out, _ = jalankan("cek-naik", "--baru", "3", "--terpasang", str(Path(d) / "tidak-ada"))
        self.assertEqual(rc, 0)
        self.assertIn("rilis pertama", out)

    def test_cek_naik_menolak_build_tidak_naik(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "manifest.signed"
            p.write_bytes(signed(10))
            self.assertEqual(jalankan("cek-naik", "--baru", "11", "--terpasang", str(p))[0], 0)
            self.assertEqual(jalankan("cek-naik", "--baru", "10", "--terpasang", str(p))[0], 1)

    def test_manifest_terpasang_rusak_menggagalkan_rilis(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "manifest.signed"
            p.write_bytes(b"rusak")
            self.assertEqual(jalankan("cek-naik", "--baru", "11", "--terpasang", str(p))[0], 1)


if __name__ == "__main__":
    unittest.main()