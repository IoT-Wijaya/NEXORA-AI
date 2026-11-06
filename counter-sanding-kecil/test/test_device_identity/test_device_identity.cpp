#include <string.h>
#include <unity.h>
#include "device_identity.h"

class FakeMac : public IMacSource
{
public:
    bool readMac(uint8_t out[6]) override
    {
        if (!ok)
            return false;
        memcpy(out, mac, 6);
        return true;
    }
    uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    bool ok = true;
};

void setUp() {}
void tearDown() {}

void test_mac_biasa_valid()
{
    const uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    TEST_ASSERT_TRUE(MacFormat::isValid(mac));
}

void test_mac_nol_semua_tidak_valid()
{
    const uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(MacFormat::isValid(mac));
}

void test_mac_multicast_tidak_valid()
{
    const uint8_t mac[6] = {0x01, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    TEST_ASSERT_FALSE(MacFormat::isValid(mac));
    const uint8_t semuaFF[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_FALSE(MacFormat::isValid(semuaFF));
}

void test_format_hex_huruf_besar_dua_belas_digit()
{
    const uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    char b[MacFormat::HEX_SIZE];
    MacFormat::toHex(mac, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("240AC4123456", b);
}

void test_format_hex_menjaga_angka_nol_di_depan()
{
    const uint8_t mac[6] = {0x02, 0x00, 0x0A, 0x00, 0x00, 0x01};
    char b[MacFormat::HEX_SIZE];
    MacFormat::toHex(mac, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("02000A000001", b);
}

void test_format_colon()
{
    const uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    char b[MacFormat::COLON_SIZE];
    MacFormat::toColon(mac, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("24:0A:C4:12:34:56", b);
}

void test_format_buffer_kecil_tetap_aman()
{
    const uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    char b[6];
    MacFormat::toHex(mac, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("240AC", b);
}

void test_begin_sukses_mengisi_mac_teks_dan_client_id()
{
    FakeMac f;
    DeviceIdentity id(f, "SandingKecil-");
    TEST_ASSERT_TRUE(id.begin());
    TEST_ASSERT_TRUE(id.isValid());
    TEST_ASSERT_EQUAL_UINT8(0x24, id.mac()[0]);
    TEST_ASSERT_EQUAL_UINT8(0x56, id.mac()[5]);
    TEST_ASSERT_EQUAL_STRING("24:0A:C4:12:34:56", id.macText());
    TEST_ASSERT_EQUAL_STRING("SandingKecil-240AC4123456", id.clientId());
}

void test_client_id_memakai_seluruh_enam_byte()
{
    // Dua chip satu batch: empat byte pertama sama, dua byte terakhir beda.
    FakeMac a;
    FakeMac b;
    b.mac[5] = 0x57;
    DeviceIdentity ida(a, "SandingKecil-");
    DeviceIdentity idb(b, "SandingKecil-");
    ida.begin();
    idb.begin();
    TEST_ASSERT_TRUE(strcmp(ida.clientId(), idb.clientId()) != 0);
}

void test_gagal_membaca_mac_berarti_tidak_valid()
{
    FakeMac f;
    f.ok = false;
    DeviceIdentity id(f, "SandingKecil-");
    TEST_ASSERT_FALSE(id.begin());
    TEST_ASSERT_FALSE(id.isValid());
    TEST_ASSERT_EQUAL_STRING("SandingKecil-INVALID", id.clientId());
}

void test_mac_tidak_valid_berarti_tidak_valid()
{
    FakeMac f;
    memset(f.mac, 0, 6);
    DeviceIdentity id(f, "SandingKecil-");
    TEST_ASSERT_FALSE(id.begin());
    TEST_ASSERT_EQUAL_STRING("SandingKecil-INVALID", id.clientId());
    TEST_ASSERT_EQUAL_STRING("00:00:00:00:00:00", id.macText());
}

void test_prefix_terlalu_panjang_tetap_aman_dan_berakhir_nul()
{
    FakeMac f;
    DeviceIdentity id(f, "PrefixYangSangatPanjangSekaliMelebihiBatas-");
    id.begin();
    TEST_ASSERT_TRUE(strlen(id.clientId()) <= 31);
}

void test_begin_dipanggil_ulang_memperbarui_hasil()
{
    FakeMac f;
    DeviceIdentity id(f, "SandingKecil-");
    TEST_ASSERT_TRUE(id.begin());
    f.ok = false;
    TEST_ASSERT_FALSE(id.begin());
    TEST_ASSERT_EQUAL_STRING("SandingKecil-INVALID", id.clientId());
    f.ok = true;
    TEST_ASSERT_TRUE(id.begin());
    TEST_ASSERT_EQUAL_STRING("SandingKecil-240AC4123456", id.clientId());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_mac_biasa_valid);
    RUN_TEST(test_mac_nol_semua_tidak_valid);
    RUN_TEST(test_mac_multicast_tidak_valid);
    RUN_TEST(test_format_hex_huruf_besar_dua_belas_digit);
    RUN_TEST(test_format_hex_menjaga_angka_nol_di_depan);
    RUN_TEST(test_format_colon);
    RUN_TEST(test_format_buffer_kecil_tetap_aman);
    RUN_TEST(test_begin_sukses_mengisi_mac_teks_dan_client_id);
    RUN_TEST(test_client_id_memakai_seluruh_enam_byte);
    RUN_TEST(test_gagal_membaca_mac_berarti_tidak_valid);
    RUN_TEST(test_mac_tidak_valid_berarti_tidak_valid);
    RUN_TEST(test_prefix_terlalu_panjang_tetap_aman_dan_berakhir_nul);
    RUN_TEST(test_begin_dipanggil_ulang_memperbarui_hasil);
    return UNITY_END();
}