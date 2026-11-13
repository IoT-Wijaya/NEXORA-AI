#pragma once
#include "iblob_store.h"
#include "ota_state_machine.h"

// IOtaStore di atas penyimpanan byte: catatan dikodekan dengan CRC, dan setiap tulisan
// dibaca ulang untuk membuktikan bahwa yang tersimpan sama dengan yang dimaksud.
class BlobOtaStore : public IOtaStore
{
public:
    BlobOtaStore(IBlobStore &blobs, const char *key);

    bool load(OtaRecord &out) override;
    bool save(const OtaRecord &record) override;

private:
    IBlobStore &blobs_;
    const char *key_;
};