#include "blob_ota_store.h"
#include "ota_record_codec.h"

namespace
{
    bool sama(const OtaRecord &a, const OtaRecord &b)
    {
        return a.state == b.state && a.attempts == b.attempts && a.result == b.result &&
               a.targetBuild == b.targetBuild && a.previousBuild == b.previousBuild;
    }
} // namespace

BlobOtaStore::BlobOtaStore(IBlobStore &blobs, const char *key) : blobs_(blobs), key_(key) {}

bool BlobOtaStore::load(OtaRecord &out)
{
    uint8_t buffer[OtaRecordCodec::SIZE];
    const int length = blobs_.read(key_, buffer, sizeof(buffer));
    if (length != (int)OtaRecordCodec::SIZE)
        return false;
    return OtaRecordCodec::decode(buffer, OtaRecordCodec::SIZE, out);
}

bool BlobOtaStore::save(const OtaRecord &record)
{
    uint8_t buffer[OtaRecordCodec::SIZE];
    OtaRecordCodec::encode(record, buffer);
    if (!blobs_.write(key_, buffer, sizeof(buffer)))
        return false;

    // Catatan ini menentukan nasib firmware: tulisan harus terbukti terbaca benar.
    OtaRecord check;
    return load(check) && sama(check, record);
}