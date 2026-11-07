#include "ota_record_codec.h"

namespace OtaRecordCodec
{

    namespace
    {
        void putU32(uint8_t *p, uint32_t v)
        {
            p[0] = (uint8_t)(v & 0xFF);
            p[1] = (uint8_t)((v >> 8) & 0xFF);
            p[2] = (uint8_t)((v >> 16) & 0xFF);
            p[3] = (uint8_t)((v >> 24) & 0xFF);
        }
        uint32_t getU32(const uint8_t *p)
        {
            return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        }
    } // namespace

    uint16_t crc16(const uint8_t *data, size_t length)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; i++)
        {
            crc ^= (uint16_t)((uint16_t)data[i] << 8);
            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
            }
        }
        return crc;
    }

    void encode(const OtaRecord &record, uint8_t out[SIZE])
    {
        out[0] = MAGIC;
        out[1] = VERSION;
        out[2] = (uint8_t)record.state;
        out[3] = record.attempts;
        out[4] = (uint8_t)record.result;
        out[5] = 0;
        putU32(out + 6, record.targetBuild);
        putU32(out + 10, record.previousBuild);
        const uint16_t crc = crc16(out, 14);
        out[14] = (uint8_t)(crc & 0xFF);
        out[15] = (uint8_t)(crc >> 8);
    }

    bool decode(const uint8_t *data, size_t length, OtaRecord &out)
    {
        if (data == nullptr || length != SIZE)
            return false;
        if (data[0] != MAGIC || data[1] != VERSION)
            return false;

        const uint16_t crc = (uint16_t)(data[14] | (data[15] << 8));
        if (crc != crc16(data, 14))
            return false;

        if (data[2] > (uint8_t)OtaState::RollingBack)
            return false;
        if (data[4] > (uint8_t)OtaResult::Abandoned)
            return false;

        OtaRecord r;
        r.state = (OtaState)data[2];
        r.attempts = data[3];
        r.result = (OtaResult)data[4];
        r.targetBuild = getU32(data + 6);
        r.previousBuild = getU32(data + 10);
        out = r;
        return true;
    }

} // namespace OtaRecordCodec