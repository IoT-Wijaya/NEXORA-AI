#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ota_record.h"

// Format tetap 16 byte:
//   byte 0       penanda (0x4F)
//   byte 1       versi format
//   byte 2       state
//   byte 3       attempts
//   byte 4       result
//   byte 5       cadangan (0)
//   byte 6..9    targetBuild, little-endian
//   byte 10..13  previousBuild, little-endian
//   byte 14..15  CRC-16 dari byte 0..13, little-endian
namespace OtaRecordCodec
{

    constexpr size_t SIZE = 16;
    constexpr uint8_t MAGIC = 0x4F;
    constexpr uint8_t VERSION = 1;

    // CRC-16/CCITT-FALSE (polinomial 0x1021, awal 0xFFFF).
    uint16_t crc16(const uint8_t *data, size_t length);

    void encode(const OtaRecord &record, uint8_t out[SIZE]);

    // false jika panjang, penanda, versi, CRC, atau nilai enum tidak valid.
    // `out` tidak diubah jika gagal.
    bool decode(const uint8_t *data, size_t length, OtaRecord &out);

} // namespace OtaRecordCodec