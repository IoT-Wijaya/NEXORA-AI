#include "signed_manifest.h"

namespace
{

    SignedManifestResult fail(SignedManifestError error)
    {
        return SignedManifestResult{error, ManifestResult{ManifestError::None, ""}};
    }

} // namespace

const char *signedManifestErrorText(SignedManifestError error)
{
    switch (error)
    {
    case SignedManifestError::None:
        return "ok";
    case SignedManifestError::TooShort:
        return "amplop terlalu pendek";
    case SignedManifestError::TooLarge:
        return "amplop terlalu besar";
    case SignedManifestError::BadSignature:
        return "tanda tangan tidak sah";
    case SignedManifestError::BadManifest:
        return "isi manifest ditolak";
    }
    return "tidak diketahui";
}

namespace SignedManifestParser
{

    SignedManifestResult parse(const uint8_t *envelope, size_t length,
                               const uint8_t publicKey[ota::PUBLIC_KEY_LEN],
                               Manifest &out)
    {
        if (envelope == nullptr || length <= ota::SIGNATURE_LEN)
        {
            return fail(SignedManifestError::TooShort);
        }
        if (length > ota::SIGNATURE_LEN + ManifestLimits::MAX_BYTES)
        {
            return fail(SignedManifestError::TooLarge);
        }

        const uint8_t *signature = envelope;
        const uint8_t *json = envelope + ota::SIGNATURE_LEN;
        const size_t jsonLen = length - ota::SIGNATURE_LEN;

        if (!ota::verifySignature(signature, publicKey, json, jsonLen))
        {
            return fail(SignedManifestError::BadSignature);
        }

        const ManifestResult parsed = ManifestParser::parse(
            reinterpret_cast<const char *>(json), jsonLen, out);
        if (!parsed.ok())
        {
            return SignedManifestResult{SignedManifestError::BadManifest, parsed};
        }
        return SignedManifestResult{SignedManifestError::None, parsed};
    }

} // namespace SignedManifestParser