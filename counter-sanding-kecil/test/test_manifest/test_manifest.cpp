#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <string>
#include <unity.h>
#include "manifest.h"

static const std::string HEX64 =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

// Setiap nilai adalah potongan JSON mentah. String kosong = kolom tidak ditulis.
struct M
{
    std::string schema = "1";
    std::string deviceType = "\"sanding-kecil\"";
    std::string board = "\"nodemcu-32s\"";
    std::string version = "\"fw-v1.0.0\"";
    std::string build = "128";
    std::string size = "340877";
    std::string sha = "\"" + HEX64 + "\"";
    std::string minFrom = "";
    std::string allow = "";
    std::string extra = "";

    std::string json() const
    {
        std::string s = "{";
        auto tambah = [&s](const char *kunci, const std::string &nilai)
        {
            if (nilai.empty())
                return;
            if (s.size() > 1)
                s += ",";
            s += "\"";
            s += kunci;
            s += "\":";
            s += nilai;
        };
        tambah("schema", schema);
        tambah("device_type", deviceType);
        tambah("board", board);
        tambah("version", version);
        tambah("build", build);
        tambah("size", size);
        tambah("sha256", sha);
        tambah("min_from_build", minFrom);
        tambah("allow_downgrade", allow);
        tambah("extra", extra);
        s += "}";
        return s;
    }
};

static ManifestResult uraikan(const M &m, Manifest &out)
{
    const std::string j = m.json();
    return ManifestParser::parse(j.c_str(), j.size(), out);
}

static void harapkan(const M &m, ManifestError galat, const char *kolom)
{
    Manifest out;
    memset(&out, 0, sizeof(out));
    out.build = 777; // penanda: tidak boleh berubah jika galat
    const ManifestResult r = uraikan(m, out);
    TEST_ASSERT_EQUAL((int)galat, (int)r.error);
    TEST_ASSERT_EQUAL_STRING(kolom, r.field);
    TEST_ASSERT_EQUAL_UINT32(777, out.build);
}

void setUp() {}
void tearDown() {}

void test_manifest_valid_terbaca_utuh()
{
    Manifest out;
    const ManifestResult r = uraikan(M(), out);
    TEST_ASSERT_TRUE(r.ok());
    TEST_ASSERT_EQUAL_UINT32(1, out.schema);
    TEST_ASSERT_EQUAL_STRING("sanding-kecil", out.deviceType);
    TEST_ASSERT_EQUAL_STRING("nodemcu-32s", out.board);
    TEST_ASSERT_EQUAL_STRING("fw-v1.0.0", out.version);
    TEST_ASSERT_EQUAL_UINT32(128, out.build);
    TEST_ASSERT_EQUAL_UINT32(340877, out.size);
    TEST_ASSERT_EQUAL_UINT8(0x01, out.sha256[0]);
    TEST_ASSERT_EQUAL_UINT8(0x23, out.sha256[1]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, out.sha256[31]);
}

void test_kolom_opsional_memakai_nilai_bawaan()
{
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(M(), out).ok());
    TEST_ASSERT_EQUAL_UINT32(0, out.minFromBuild);
    TEST_ASSERT_FALSE(out.allowDowngrade);
}

void test_kolom_opsional_yang_ditulis_terbaca()
{
    M m;
    m.minFrom = "100";
    m.allow = "true";
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(m, out).ok());
    TEST_ASSERT_EQUAL_UINT32(100, out.minFromBuild);
    TEST_ASSERT_TRUE(out.allowDowngrade);
}

void test_hex_huruf_besar_diterima_dan_sama_hasilnya()
{
    M m;
    std::string besar = HEX64;
    std::transform(besar.begin(), besar.end(), besar.begin(), ::toupper);
    m.sha = "\"" + besar + "\"";
    Manifest a, b;
    TEST_ASSERT_TRUE(uraikan(m, a).ok());
    TEST_ASSERT_TRUE(uraikan(M(), b).ok());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(b.sha256, a.sha256, 32);
}

void test_kolom_tak_dikenal_diabaikan()
{
    M m;
    m.extra = "{\"apa\":[1,2,3]}";
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(m, out).ok());
}

void test_kosong_null_dan_terlalu_besar()
{
    Manifest out;
    TEST_ASSERT_EQUAL((int)ManifestError::EmptyOrTooLarge, (int)ManifestParser::parse("", 0, out).error);
    TEST_ASSERT_EQUAL((int)ManifestError::EmptyOrTooLarge, (int)ManifestParser::parse(nullptr, 10, out).error);
    const std::string besar(ManifestLimits::MAX_BYTES + 1, ' ');
    TEST_ASSERT_EQUAL((int)ManifestError::EmptyOrTooLarge,
                      (int)ManifestParser::parse(besar.c_str(), besar.size(), out).error);
}

void test_json_rusak_bukan_objek_dan_terpotong()
{
    Manifest out;
    const char *rusak = "bukan json";
    TEST_ASSERT_EQUAL((int)ManifestError::InvalidJson, (int)ManifestParser::parse(rusak, strlen(rusak), out).error);
    const char *larik = "[1,2,3]";
    TEST_ASSERT_EQUAL((int)ManifestError::InvalidJson, (int)ManifestParser::parse(larik, strlen(larik), out).error);
    const std::string utuh = M().json();
    TEST_ASSERT_EQUAL((int)ManifestError::InvalidJson,
                      (int)ManifestParser::parse(utuh.c_str(), utuh.size() - 5, out).error);
}

void test_hanya_membaca_sebanyak_length()
{
    // Teks setelah `length` byte tidak boleh ikut dibaca.
    const std::string j = M().json() + "sampah";
    Manifest out;
    TEST_ASSERT_TRUE(ManifestParser::parse(j.c_str(), j.size() - 6, out).ok());
}

void test_schema_tidak_didukung_dan_salah_tipe_dan_hilang()
{
    M m;
    m.schema = "2";
    harapkan(m, ManifestError::UnsupportedSchema, "schema");
    m.schema = "\"1\"";
    harapkan(m, ManifestError::BadField, "schema");
    m.schema = "";
    harapkan(m, ManifestError::MissingField, "schema");
}

void test_setiap_kolom_wajib_dilaporkan_namanya_jika_hilang()
{
    M m;
    m = M();
    m.deviceType = "";
    harapkan(m, ManifestError::MissingField, "device_type");
    m = M();
    m.board = "";
    harapkan(m, ManifestError::MissingField, "board");
    m = M();
    m.version = "";
    harapkan(m, ManifestError::MissingField, "version");
    m = M();
    m.build = "";
    harapkan(m, ManifestError::MissingField, "build");
    m = M();
    m.size = "";
    harapkan(m, ManifestError::MissingField, "size");
    m = M();
    m.sha = "";
    harapkan(m, ManifestError::MissingField, "sha256");
}

void test_build_tidak_valid()
{
    M m;
    const char *buruk[] = {"\"128\"", "0", "-1", "1.5", "4294967296", "true", "[1]"};
    for (const char *b : buruk)
    {
        m.build = b;
        harapkan(m, ManifestError::BadField, "build");
    }
    m.build = "4294967295";
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(m, out).ok());
    TEST_ASSERT_EQUAL_UINT32(4294967295UL, out.build);
}

void test_size_tidak_valid()
{
    M m;
    const char *buruk[] = {"0", "-5", "\"100\"", "16777217"};
    for (const char *s : buruk)
    {
        m.size = s;
        harapkan(m, ManifestError::BadField, "size");
    }
    m.size = "16777216";
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(m, out).ok());
}

void test_sha256_tidak_valid()
{
    M m;
    const std::string pendek = HEX64.substr(0, 63);
    const std::string panjang = HEX64 + "0";
    std::string bukanHex = HEX64;
    bukanHex[10] = 'g';
    const std::string buruk[] = {"\"\"", "\"" + pendek + "\"", "\"" + panjang + "\"",
                                 "\"" + bukanHex + "\"", "12345"};
    for (const std::string &s : buruk)
    {
        m.sha = s;
        harapkan(m, ManifestError::BadField, "sha256");
    }
}

void test_device_type_dan_board_hanya_huruf_kecil_angka_strip()
{
    M m;
    const char *buruk[] = {"\"\"", "\"Sanding\"", "\"sanding kecil\"", "\"sanding_kecil\"",
                           "\"abcdefghijklmnopqrstuvwx\"", "123"};
    for (const char *t : buruk)
    {
        m = M();
        m.deviceType = t;
        harapkan(m, ManifestError::BadField, "device_type");
        m = M();
        m.board = t;
        harapkan(m, ManifestError::BadField, "board");
    }
    m = M();
    m.deviceType = "\"abcdefghijklmnopqrstuvw\""; // tepat 23
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(m, out).ok());
}

void test_version_karakter_dan_panjang()
{
    M m;
    const std::string panjang64 = "\"" + std::string(64, 'v') + "\"";
    const char *buruk[] = {"\"\"", "\"fw v1\"", "\"fw\\\"v1\"", "\"fw/v1\"", "7"};
    for (const char *v : buruk)
    {
        m.version = v;
        harapkan(m, ManifestError::BadField, "version");
    }
    m.version = panjang64;
    harapkan(m, ManifestError::BadField, "version");
    m.version = "\"" + std::string(63, 'v') + "\"";
    Manifest out;
    TEST_ASSERT_TRUE(uraikan(m, out).ok());
}

void test_kolom_opsional_salah_tipe_ditolak()
{
    M m;
    m.minFrom = "\"seratus\"";
    harapkan(m, ManifestError::BadField, "min_from_build");
    m = M();
    m.minFrom = "-1";
    harapkan(m, ManifestError::BadField, "min_from_build");
    m = M();
    m.allow = "\"ya\"";
    harapkan(m, ManifestError::BadField, "allow_downgrade");
    m = M();
    m.allow = "1";
    harapkan(m, ManifestError::BadField, "allow_downgrade");
}

void test_teks_galat_tersedia_untuk_semua_jenis()
{
    TEST_ASSERT_EQUAL_STRING("ok", manifestErrorText(ManifestError::None));
    TEST_ASSERT_EQUAL_STRING("JSON rusak", manifestErrorText(ManifestError::InvalidJson));
    TEST_ASSERT_EQUAL_STRING("kolom wajib tidak ada", manifestErrorText(ManifestError::MissingField));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_manifest_valid_terbaca_utuh);
    RUN_TEST(test_kolom_opsional_memakai_nilai_bawaan);
    RUN_TEST(test_kolom_opsional_yang_ditulis_terbaca);
    RUN_TEST(test_hex_huruf_besar_diterima_dan_sama_hasilnya);
    RUN_TEST(test_kolom_tak_dikenal_diabaikan);
    RUN_TEST(test_kosong_null_dan_terlalu_besar);
    RUN_TEST(test_json_rusak_bukan_objek_dan_terpotong);
    RUN_TEST(test_hanya_membaca_sebanyak_length);
    RUN_TEST(test_schema_tidak_didukung_dan_salah_tipe_dan_hilang);
    RUN_TEST(test_setiap_kolom_wajib_dilaporkan_namanya_jika_hilang);
    RUN_TEST(test_build_tidak_valid);
    RUN_TEST(test_size_tidak_valid);
    RUN_TEST(test_sha256_tidak_valid);
    RUN_TEST(test_device_type_dan_board_hanya_huruf_kecil_angka_strip);
    RUN_TEST(test_version_karakter_dan_panjang);
    RUN_TEST(test_kolom_opsional_salah_tipe_ditolak);
    RUN_TEST(test_teks_galat_tersedia_untuk_semua_jenis);
    return UNITY_END();
}