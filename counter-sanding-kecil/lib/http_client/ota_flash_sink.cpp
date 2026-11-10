#include "ota_flash_sink.h"

bool OtaFlashSink::begin()
{
    if (open_ || ready_)
        return fail("begin() dipanggil dua kali");
    if (!flash_.begin(manifest_.size))
        return fail(flash_.lastError());
    verifier_.begin();
    open_ = true;
    return true;
}

bool OtaFlashSink::write(const uint8_t *data, size_t len)
{
    if (!open_)
        return fail("write() sebelum begin()");
    if (!verifier_.update(data, len))
        return fail("data melebihi ukuran manifest");
    if (!flash_.write(data, len))
        return fail(flash_.lastError());
    return true;
}

bool OtaFlashSink::finish()
{
    if (!open_)
        return fail("finish() tanpa begin()");
    const VerifyResult r = verifier_.finish();
    if (r != VerifyResult::Ok)
    {
        abort();
        return fail(verifyResultCode(r));
    }
    open_ = false; // flash.end() melepas handle apa pun hasilnya
    if (!flash_.end())
        return fail(flash_.lastError());
    ready_ = true;
    return true;
}

bool OtaFlashSink::activate()
{
    if (!ready_)
        return fail("activate() sebelum finish() berhasil");
    if (!flash_.setBootToNew())
        return fail(flash_.lastError());
    return true;
}

void OtaFlashSink::abort()
{
    if (open_)
    {
        flash_.abort();
        open_ = false;
    }
}