#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unity.h>
#include "sha256.h"

static std::string hex(const uint8_t d[32])
{
    char b[65];
    for (int i = 0; i < 32; i++)
        snprintf(b + 2 * i, 3, "%02x", d[i]);
    return std::string(b);
}

static std::string hashSekaligus(const std::string &teks)
{
    Sha256 h;
    h.update((const uint8_t *)teks.data(), teks.size());
    uint8_t d[32];
    h.finish(d);
    return hex(d);
}

void setUp() {}
void tearDown() {}

void test_vektor_kosong()
{
    TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                             hashSekaligus("").c_str());
}

void test_vektor_abc()
{
    TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                             hashSekaligus("abc").c_str());
}

void test_vektor_dua_blok_dengan_padding_meluap()
{
    TEST_ASSERT_EQUAL_STRING(
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        hashSekaligus("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").c_str());
}

void test_vektor_sejuta_huruf_a_dalam_potongan_seribu()
{
    Sha256 h;
    const std::string potongan(1000, 'a');
    for (int i = 0; i < 1000; i++)
        h.update((const uint8_t *)potongan.data(), potongan.size());
    uint8_t d[32];
    h.finish(d);
    TEST_ASSERT_EQUAL_STRING("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
                             hex(d).c_str());
}

void test_ukuran_potongan_apa_pun_menghasilkan_hash_sama()
{
    std::string data;
    for (int i = 0; i < 1000; i++)
        data += (char)(i * 7 + 3);
    const std::string acuan = hashSekaligus(data);

    const size_t ukuran[] = {1, 2, 7, 55, 56, 57, 63, 64, 65, 100, 128, 999};
    for (size_t u : ukuran)
    {
        Sha256 h;
        for (size_t i = 0; i < data.size(); i += u)
        {
            const size_t n = (data.size() - i < u) ? data.size() - i : u;
            h.update((const uint8_t *)data.data() + i, n);
        }
        uint8_t d[32];
        h.finish(d);
        TEST_ASSERT_EQUAL_STRING(acuan.c_str(), hex(d).c_str());
    }
}

void test_panjang_di_sekitar_batas_padding_konsisten()
{
    // Panjang 54 sampai 66 melewati semua kasus padding. Hasil harus beda tiap panjang
    // dan sama jika dihitung ulang.
    std::vector<std::string> hasil;
    for (size_t len = 54; len <= 66; len++)
    {
        const std::string teks(len, 'x');
        const std::string a = hashSekaligus(teks);
        TEST_ASSERT_EQUAL_STRING(a.c_str(), hashSekaligus(teks).c_str());
        for (const std::string &lama : hasil)
            TEST_ASSERT_TRUE(lama != a);
        hasil.push_back(a);
    }
}

void test_reset_memulai_ulang_dari_nol()
{
    Sha256 h;
    h.update((const uint8_t *)"sampah", 6);
    h.reset();
    h.update((const uint8_t *)"abc", 3);
    uint8_t d[32];
    h.finish(d);
    TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                             hex(d).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_vektor_kosong);
    RUN_TEST(test_vektor_abc);
    RUN_TEST(test_vektor_dua_blok_dengan_padding_meluap);
    RUN_TEST(test_vektor_sejuta_huruf_a_dalam_potongan_seribu);
    RUN_TEST(test_ukuran_potongan_apa_pun_menghasilkan_hash_sama);
    RUN_TEST(test_panjang_di_sekitar_batas_padding_konsisten);
    RUN_TEST(test_reset_memulai_ulang_dari_nol);
    return UNITY_END();
}