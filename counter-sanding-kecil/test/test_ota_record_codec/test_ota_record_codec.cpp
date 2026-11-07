#include <string.h>
#include <unity.h>
#include "ota_record_codec.h"

static OtaRecord contoh()
{
    return OtaRecord{OtaState::Pending, 2, OtaResult::RolledBackTimeout, 0x01020304u, 0x0A0B0C0Du};
}

// Menghitung ulang CRC agar nilai yang diubah sengaja tetap lolos pemeriksaan CRC.
static void segel(uint8_t b[16])
{
    const uint16_t crc = OtaRecordCodec::crc16(b, 14);
    b[14] = (uint8_t)(crc & 0xFF);
    b[15] = (uint8_t)(crc >> 8);
}

void setUp() {}
void tearDown() {}

void test_crc16_cocok_dengan_nilai_acuan_standar()
{
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, OtaRecordCodec::crc16(data, sizeof(data)));
}

void test_tata_letak_byte_sesuai_dokumentasi()
{
    uint8_t b[16];
    OtaRecordCodec::encode(contoh(), b);
    TEST_ASSERT_EQUAL_UINT8(0x4F, b[0]);
    TEST_ASSERT_EQUAL_UINT8(1, b[1]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)OtaState::Pending, b[2]);
    TEST_ASSERT_EQUAL_UINT8(2, b[3]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)OtaResult::RolledBackTimeout, b[4]);
    TEST_ASSERT_EQUAL_UINT8(0, b[5]);
    TEST_ASSERT_EQUAL_UINT8(0x04, b[6]);
    TEST_ASSERT_EQUAL_UINT8(0x03, b[7]);
    TEST_ASSERT_EQUAL_UINT8(0x02, b[8]);
    TEST_ASSERT_EQUAL_UINT8(0x01, b[9]);
    TEST_ASSERT_EQUAL_UINT8(0x0D, b[10]);
    TEST_ASSERT_EQUAL_UINT8(0x0C, b[11]);
    TEST_ASSERT_EQUAL_UINT8(0x0B, b[12]);
    TEST_ASSERT_EQUAL_UINT8(0x0A, b[13]);
    const uint16_t crc = OtaRecordCodec::crc16(b, 14);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(crc & 0xFF), b[14]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(crc >> 8), b[15]);
}

void test_bolak_balik_semua_state_dan_hasil()
{
    const OtaState states[] = {OtaState::Idle, OtaState::Requested, OtaState::Pending, OtaState::RollingBack};
    for (OtaState s : states)
    {
        for (int r = 0; r <= (int)OtaResult::Abandoned; r++)
        {
            const OtaRecord asli{s, 7, (OtaResult)r, 4294967295u, 1u};
            uint8_t b[16];
            OtaRecordCodec::encode(asli, b);
            OtaRecord hasil = freshOtaRecord();
            TEST_ASSERT_TRUE(OtaRecordCodec::decode(b, sizeof(b), hasil));
            TEST_ASSERT_EQUAL((int)asli.state, (int)hasil.state);
            TEST_ASSERT_EQUAL_UINT8(asli.attempts, hasil.attempts);
            TEST_ASSERT_EQUAL((int)asli.result, (int)hasil.result);
            TEST_ASSERT_EQUAL_UINT32(asli.targetBuild, hasil.targetBuild);
            TEST_ASSERT_EQUAL_UINT32(asli.previousBuild, hasil.previousBuild);
        }
    }
}

void test_satu_bit_berubah_di_byte_mana_pun_ditolak()
{
    uint8_t asli[16];
    OtaRecordCodec::encode(contoh(), asli);
    for (int byte = 0; byte < 16; byte++)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            uint8_t b[16];
            memcpy(b, asli, sizeof(b));
            b[byte] ^= (uint8_t)(1u << bit);
            OtaRecord hasil = freshOtaRecord();
            TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, sizeof(b), hasil));
        }
    }
}

void test_panjang_salah_dan_null_ditolak()
{
    uint8_t b[17] = {0};
    OtaRecordCodec::encode(contoh(), b);
    OtaRecord hasil = freshOtaRecord();
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, 15, hasil));
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, 17, hasil));
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, 0, hasil));
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(nullptr, 16, hasil));
}

void test_penanda_dan_versi_salah_ditolak_walau_crc_benar()
{
    uint8_t b[16];
    OtaRecord hasil = freshOtaRecord();
    OtaRecordCodec::encode(contoh(), b);
    b[0] = 0x00;
    segel(b);
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, sizeof(b), hasil));

    OtaRecordCodec::encode(contoh(), b);
    b[1] = 2;
    segel(b);
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, sizeof(b), hasil));
}

void test_nilai_enum_di_luar_rentang_ditolak_walau_crc_benar()
{
    uint8_t b[16];
    OtaRecord hasil = freshOtaRecord();
    OtaRecordCodec::encode(contoh(), b);
    b[2] = 4; // state tidak dikenal
    segel(b);
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, sizeof(b), hasil));

    OtaRecordCodec::encode(contoh(), b);
    b[4] = (uint8_t)OtaResult::Abandoned + 1; // result tidak dikenal
    segel(b);
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, sizeof(b), hasil));
}

void test_hasil_tidak_berubah_jika_decode_gagal()
{
    uint8_t b[16];
    OtaRecordCodec::encode(contoh(), b);
    b[3] ^= 0x01;
    OtaRecord hasil{OtaState::Requested, 9, OtaResult::Confirmed, 777, 888};
    TEST_ASSERT_FALSE(OtaRecordCodec::decode(b, sizeof(b), hasil));
    TEST_ASSERT_EQUAL_UINT32(777, hasil.targetBuild);
    TEST_ASSERT_EQUAL_UINT8(9, hasil.attempts);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc16_cocok_dengan_nilai_acuan_standar);
    RUN_TEST(test_tata_letak_byte_sesuai_dokumentasi);
    RUN_TEST(test_bolak_balik_semua_state_dan_hasil);
    RUN_TEST(test_satu_bit_berubah_di_byte_mana_pun_ditolak);
    RUN_TEST(test_panjang_salah_dan_null_ditolak);
    RUN_TEST(test_penanda_dan_versi_salah_ditolak_walau_crc_benar);
    RUN_TEST(test_nilai_enum_di_luar_rentang_ditolak_walau_crc_benar);
    RUN_TEST(test_hasil_tidak_berubah_jika_decode_gagal);
    return UNITY_END();
}