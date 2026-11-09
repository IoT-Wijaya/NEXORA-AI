import hashlib
import json
import unittest

from make_manifest import ManifestError, build_manifest, DEFAULT_SLOT_BYTES

FW = b"\xe9" + bytes(range(256)) * 4


def ok(**kw):
    args = dict(version="fw-v1.0.0", build=12)
    args.update(kw)
    return build_manifest(FW, **args)


class MakeManifestTest(unittest.TestCase):
    def test_kolom_dan_hash_benar(self):
        d = json.loads(ok())
        self.assertEqual(d["schema"], 1)
        self.assertEqual(d["device_type"], "sanding-kecil")
        self.assertEqual(d["board"], "nodemcu-32s")
        self.assertEqual(d["version"], "fw-v1.0.0")
        self.assertEqual(d["build"], 12)
        self.assertEqual(d["size"], len(FW))
        self.assertEqual(d["sha256"], hashlib.sha256(FW).hexdigest())

    def test_kolom_opsional_tidak_ada_bila_tidak_diminta(self):
        d = json.loads(ok())
        self.assertNotIn("min_from_build", d)
        self.assertNotIn("allow_downgrade", d)

    def test_kolom_opsional_muncul_bila_diminta(self):
        d = json.loads(ok(min_from_build=5, allow_downgrade=True))
        self.assertEqual(d["min_from_build"], 5)
        self.assertTrue(d["allow_downgrade"])

    def test_keluaran_ringkas_tanpa_baris_baru(self):
        out = ok()
        self.assertNotIn(b"\n", out)
        self.assertNotIn(b" ", out)

    def test_keluaran_sama_untuk_masukan_sama(self):
        self.assertEqual(ok(), ok())

    def test_firmware_kosong_ditolak(self):
        with self.assertRaises(ManifestError):
            build_manifest(b"", version="v1", build=1)

    def test_melebihi_slot_ditolak_tepat_di_batas_diterima(self):
        big = bytes(DEFAULT_SLOT_BYTES + 1)
        with self.assertRaises(ManifestError):
            build_manifest(big, version="v1", build=1)
        build_manifest(bytes(DEFAULT_SLOT_BYTES), version="v1", build=1)

    def test_build_nol_ditolak(self):
        with self.assertRaises(ManifestError):
            ok(build=0)

    def test_min_from_build_harus_lebih_kecil_dari_build(self):
        with self.assertRaises(ManifestError):
            ok(min_from_build=12)
        with self.assertRaises(ManifestError):
            ok(min_from_build=13)
        ok(min_from_build=11)

    def test_version_tidak_valid_ditolak(self):
        for v in ["", "a b", "v1/2", 'v"1', "x" * 64, "v1\n"]:
            with self.subTest(v=v), self.assertRaises(ManifestError):
                ok(version=v)
        ok(version="x" * 63)

    def test_device_type_dan_board_tidak_valid_ditolak(self):
        for t in ["", "Sanding", "sanding_kecil", "a" * 24, "san ding"]:
            with self.subTest(t=t):
                with self.assertRaises(ManifestError):
                    ok(device_type=t)
                with self.assertRaises(ManifestError):
                    ok(board=t)
        ok(device_type="a" * 23)


if __name__ == "__main__":
    unittest.main()