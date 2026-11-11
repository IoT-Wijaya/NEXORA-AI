#include <string.h>
#include <string>
#include <unity.h>

#include "line_router.h"

static std::string forwarded;
static void feedAll(LineRouter &r, const char *s)
{
    for (; *s; ++s)
        r.feed(*s);
}
static LineRouter makeRouter()
{
    return LineRouter([](char c)
                      { forwarded.push_back(c); });
}

void setUp() { forwarded.clear(); }
void tearDown() {}

void test_baris_biasa_diteruskan_utuh_dengan_newline()
{
    LineRouter r = makeRouter();
    feedAll(r, "show\n");
    TEST_ASSERT_EQUAL_STRING("show\n", forwarded.c_str());
}

void test_crlf_tidak_menggandakan_baris()
{
    LineRouter r = makeRouter();
    feedAll(r, "show\r\nhelp\r\n");
    TEST_ASSERT_EQUAL_STRING("show\nhelp\n", forwarded.c_str());
}

void test_baris_kosong_diabaikan()
{
    LineRouter r = makeRouter();
    feedAll(r, "\n\r\n\n");
    TEST_ASSERT_EQUAL_STRING("", forwarded.c_str());
}

void test_penangan_mengambil_baris_dan_tidak_diteruskan()
{
    LineRouter r = makeRouter();
    std::string dilihat;
    r.addHandler([&](const char *l)
                 { dilihat = l; return strncmp(l, "ota", 3) == 0; });
    feedAll(r, "ota 2\n");
    TEST_ASSERT_EQUAL_STRING("ota 2", dilihat.c_str());
    TEST_ASSERT_EQUAL_STRING("", forwarded.c_str());
}

void test_penangan_menolak_baris_maka_diteruskan()
{
    LineRouter r = makeRouter();
    r.addHandler([](const char *l)
                 { return strncmp(l, "ota", 3) == 0; });
    feedAll(r, "set mqtt_host x\n");
    TEST_ASSERT_EQUAL_STRING("set mqtt_host x\n", forwarded.c_str());
}

void test_penangan_pertama_yang_menerima_menang()
{
    LineRouter r = makeRouter();
    int a = 0, b = 0;
    r.addHandler([&](const char *)
                 { ++a; return true; });
    r.addHandler([&](const char *)
                 { ++b; return true; });
    feedAll(r, "x\n");
    TEST_ASSERT_EQUAL(1, a);
    TEST_ASSERT_EQUAL(0, b);
}

void test_daftar_penangan_ada_batasnya()
{
    LineRouter r = makeRouter();
    for (size_t i = 0; i < LineRouter::MAX_HANDLERS; ++i)
        TEST_ASSERT_TRUE(r.addHandler([](const char *)
                                      { return false; }));
    TEST_ASSERT_FALSE(r.addHandler([](const char *)
                                   { return false; }));
    TEST_ASSERT_FALSE(LineRouter([](char) {}).addHandler(nullptr));
}

void test_baris_setelah_baris_yang_diambil_tetap_diteruskan()
{
    LineRouter r = makeRouter();
    r.addHandler([](const char *l)
                 { return strncmp(l, "ota", 3) == 0; });
    feedAll(r, "ota 2\nshow\n");
    TEST_ASSERT_EQUAL_STRING("show\n", forwarded.c_str());
}

void test_baris_tepat_batas_masih_ditangani_penangan()
{
    LineRouter r = makeRouter();
    size_t panjang = 0;
    r.addHandler([&](const char *l)
                 { panjang = strlen(l); return true; });
    std::string s(LineRouter::MAX_LINE, 'a');
    feedAll(r, (s + "\n").c_str());
    TEST_ASSERT_EQUAL(LineRouter::MAX_LINE, panjang);
    TEST_ASSERT_EQUAL_STRING("", forwarded.c_str());
}

void test_baris_lebih_panjang_diteruskan_utuh_agar_konsol_melaporkannya()
{
    LineRouter r = makeRouter();
    bool dipanggil = false;
    r.addHandler([&](const char *)
                 { dipanggil = true; return true; });
    std::string s(LineRouter::MAX_LINE + 30, 'b');
    feedAll(r, (s + "\n").c_str());
    TEST_ASSERT_FALSE(dipanggil);
    TEST_ASSERT_EQUAL_STRING((s + "\n").c_str(), forwarded.c_str());
}

void test_setelah_baris_panjang_router_kembali_normal()
{
    LineRouter r = makeRouter();
    int dipanggil = 0;
    r.addHandler([&](const char *)
                 { ++dipanggil; return true; });
    std::string s(LineRouter::MAX_LINE + 5, 'c');
    feedAll(r, (s + "\nota 2\n").c_str());
    TEST_ASSERT_EQUAL(1, dipanggil);
}

void test_penangan_boleh_dipanggil_ulang_untuk_baris_berikutnya()
{
    LineRouter r = makeRouter();
    int n = 0;
    r.addHandler([&](const char *)
                 { ++n; return true; });
    feedAll(r, "a\nb\nc\n");
    TEST_ASSERT_EQUAL(3, n);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_baris_biasa_diteruskan_utuh_dengan_newline);
    RUN_TEST(test_crlf_tidak_menggandakan_baris);
    RUN_TEST(test_baris_kosong_diabaikan);
    RUN_TEST(test_penangan_mengambil_baris_dan_tidak_diteruskan);
    RUN_TEST(test_penangan_menolak_baris_maka_diteruskan);
    RUN_TEST(test_penangan_pertama_yang_menerima_menang);
    RUN_TEST(test_daftar_penangan_ada_batasnya);
    RUN_TEST(test_baris_setelah_baris_yang_diambil_tetap_diteruskan);
    RUN_TEST(test_baris_tepat_batas_masih_ditangani_penangan);
    RUN_TEST(test_baris_lebih_panjang_diteruskan_utuh_agar_konsol_melaporkannya);
    RUN_TEST(test_setelah_baris_panjang_router_kembali_normal);
    RUN_TEST(test_penangan_boleh_dipanggil_ulang_untuk_baris_berikutnya);
    return UNITY_END();
}