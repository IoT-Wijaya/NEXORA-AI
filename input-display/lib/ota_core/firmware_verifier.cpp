#include "firmware_verifier.h"
#include <string.h>

const char *verifyResultCode(VerifyResult result)
{
    switch (result)
    {
    case VerifyResult::Ok:
        return "ok";
    case VerifyResult::SizeMismatch:
        return "size_mismatch";
    case VerifyResult::TooMuchData:
        return "too_much_data";
    case VerifyResult::HashMismatch:
        return "hash_mismatch";
    }
    return "unknown";
}

FirmwareVerifier::FirmwareVerifier(const Manifest &manifest)
    : expectedSize_(manifest.size), received_(0), overflow_(false), finished_(false),
      result_(VerifyResult::Ok)
{
    memcpy(expectedHash_, manifest.sha256, sizeof(expectedHash_));
}

void FirmwareVerifier::begin()
{
    hash_.reset();
    received_ = 0;
    overflow_ = false;
    finished_ = false;
    result_ = VerifyResult::Ok;
}

bool FirmwareVerifier::update(const uint8_t *data, size_t length)
{
    if (finished_ || overflow_)
        return false;
    if (length == 0)
        return true;
    if (data == nullptr)
        return false;

    // Invarian: received_ <= expectedSize_, sehingga pengurangan ini tidak pernah melingkar.
    if (length > (size_t)(expectedSize_ - received_))
    {
        overflow_ = true;
        return false;
    }
    hash_.update(data, length);
    received_ += (uint32_t)length;
    return true;
}

VerifyResult FirmwareVerifier::finish()
{
    if (finished_)
        return result_;
    finished_ = true;

    if (overflow_)
    {
        result_ = VerifyResult::TooMuchData;
    }
    else if (received_ != expectedSize_)
    {
        result_ = VerifyResult::SizeMismatch;
    }
    else
    {
        uint8_t actual[Sha256::DIGEST_SIZE];
        hash_.finish(actual);
        uint8_t difference = 0; // dibandingkan penuh, tanpa berhenti di byte pertama yang beda
        for (size_t i = 0; i < sizeof(actual); i++)
            difference |= (uint8_t)(actual[i] ^ expectedHash_[i]);
        result_ = (difference == 0) ? VerifyResult::Ok : VerifyResult::HashMismatch;
    }
    return result_;
}