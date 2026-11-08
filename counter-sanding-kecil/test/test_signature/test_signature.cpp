#include <string.h>
#include <unity.h>

#include "signature.h"

// Vektor uji dibuat dengan: openssl genpkey -algorithm ED25519
// Kunci ini HANYA untuk tes dan tidak dipakai di perangkat mana pun.
static const uint8_t PUB[32] = {
    0xdd, 0x2d, 0x63, 0x1d, 0x4e, 0xd5, 0x14, 0x47,
    0xd7, 0x37, 0xb8, 0x38, 0x10, 0x5e, 0x7f, 0xd8,
    0x49, 0x58, 0xa5, 0xc3, 0x1e, 0xc4, 0x33, 0x13,
    0x37, 0x6e, 0x74, 0xe8, 0x16, 0xb2, 0x8c, 0x70
};
static const uint8_t SIG[64] = {
    0x97, 0xe5, 0xfc, 0x7e, 0x4b, 0xb5, 0x64, 0xc7,
    0x36, 0x46, 0x0d, 0x58, 0x50, 0x7e, 0x12, 0xc6,
    0xe3, 0x7c, 0x22, 0xe5, 0xde, 0x03, 0x86, 0x84,
    0xf6, 0xc9, 0x28, 0x96, 0x93, 0xd4, 0xeb, 0xf7,
    0x10, 0x89, 0x12, 0x86, 0x83, 0x7c, 0xff, 0xbb,
    0x0a, 0x7d, 0x08, 0xd5, 0xa2, 0x92, 0xd4, 0x35,
    0xc3, 0x04, 0x92, 0x00, 0x5a, 0x32, 0x55, 0x10,
    0x76, 0x82, 0x02, 0x49, 0xa1, 0xbb, 0x1d, 0x0a
};
static const char MSG[] = "{\"schema\":1,\"version\":\"1.0.0\",\"build\":7}";

static const uint8_t* msg() { return reinterpret_cast<const uint8_t*>(MSG); }
static size_t msgLen() { return sizeof(MSG) - 1; }

void setUp() {}
void tearDown() {}

void test_tanda_tangan_sah_diterima() {
    TEST_ASSERT_TRUE(ota::verifySignature(SIG, PUB, msg(), msgLen()));
}

void test_satu_bit_pesan_berubah_ditolak() {
    uint8_t salinan[sizeof(MSG)];
    memcpy(salinan, MSG, sizeof(MSG));
    salinan[10] ^= 0x01;
    TEST_ASSERT_FALSE(ota::verifySignature(SIG, PUB, salinan, msgLen()));
}

void test_pesan_dipotong_ditolak() {
    TEST_ASSERT_FALSE(ota::verifySignature(SIG, PUB, msg(), msgLen() - 1));
}

void test_pesan_ditambah_ditolak() {
    uint8_t lebih[sizeof(MSG) + 1];
    memcpy(lebih, MSG, sizeof(MSG) - 1);
    lebih[sizeof(MSG) - 1] = ' ';
    TEST_ASSERT_FALSE(ota::verifySignature(SIG, PUB, lebih, msgLen() + 1));
}

void test_satu_bit_tanda_tangan_berubah_ditolak() {
    uint8_t rusak[64];
    memcpy(rusak, SIG, 64);
    rusak[0] ^= 0x01;
    TEST_ASSERT_FALSE(ota::verifySignature(rusak, PUB, msg(), msgLen()));
}

void test_kunci_publik_lain_ditolak() {
    uint8_t lain[32];
    memcpy(lain, PUB, 32);
    lain[31] ^= 0x01;
    TEST_ASSERT_FALSE(ota::verifySignature(SIG, lain, msg(), msgLen()));
}

void test_tanda_tangan_nol_ditolak() {
    uint8_t nol[64] = {0};
    TEST_ASSERT_FALSE(ota::verifySignature(nol, PUB, msg(), msgLen()));
}

void test_kunci_publik_nol_ditolak_walau_tanda_tangan_nol() {
    // Placeholder kunci yang lupa diisi tidak boleh membuat pemalsuan lolos.
    uint8_t nolKunci[32] = {0};
    uint8_t nolSig[64] = {0};
    TEST_ASSERT_FALSE(ota::verifySignature(nolSig, nolKunci, msg(), msgLen()));
}

void test_pointer_null_ditolak_tanpa_crash() {
    TEST_ASSERT_FALSE(ota::verifySignature(nullptr, PUB, msg(), msgLen()));
    TEST_ASSERT_FALSE(ota::verifySignature(SIG, nullptr, msg(), msgLen()));
    TEST_ASSERT_FALSE(ota::verifySignature(SIG, PUB, nullptr, 5));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tanda_tangan_sah_diterima);
    RUN_TEST(test_satu_bit_pesan_berubah_ditolak);
    RUN_TEST(test_pesan_dipotong_ditolak);
    RUN_TEST(test_pesan_ditambah_ditolak);
    RUN_TEST(test_satu_bit_tanda_tangan_berubah_ditolak);
    RUN_TEST(test_kunci_publik_lain_ditolak);
    RUN_TEST(test_tanda_tangan_nol_ditolak);
    RUN_TEST(test_kunci_publik_nol_ditolak_walau_tanda_tangan_nol);
    RUN_TEST(test_pointer_null_ditolak_tanpa_crash);
    return UNITY_END();
}