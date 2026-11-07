#include "manifest.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace
{

    ManifestResult missing(const char *field) { return ManifestResult{ManifestError::MissingField, field}; }
    ManifestResult bad(const char *field) { return ManifestResult{ManifestError::BadField, field}; }

    bool isTypeChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; }
    bool isVersionChar(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               c == '.' || c == '-' || c == '_' || c == '+';
    }

    bool validType(const char *s)
    {
        const size_t n = strlen(s);
        if (n < 1 || n > ManifestLimits::MAX_TYPE_LEN)
            return false;
        for (size_t i = 0; i < n; i++)
        {
            if (!isTypeChar(s[i]))
                return false;
        }
        return true;
    }

    bool validVersion(const char *s)
    {
        const size_t n = strlen(s);
        if (n < 1 || n > ManifestLimits::MAX_VERSION_LEN)
            return false;
        for (size_t i = 0; i < n; i++)
        {
            if (!isVersionChar(s[i]))
                return false;
        }
        return true;
    }

    int hexValue(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    bool validSha256(const char *s)
    {
        if (strlen(s) != 64)
            return false;
        for (size_t i = 0; i < 64; i++)
        {
            if (hexValue(s[i]) < 0)
                return false;
        }
        return true;
    }

    void decodeSha256(const char *s, uint8_t out[32])
    {
        for (size_t i = 0; i < 32; i++)
        {
            out[i] = (uint8_t)((hexValue(s[2 * i]) << 4) | hexValue(s[2 * i + 1]));
        }
    }

    // Kolom teks wajib. Mengembalikan nullptr dan mengisi `error` jika gagal.
    const char *readText(JsonObjectConst root, const char *key, bool (*valid)(const char *),
                         ManifestResult &error)
    {
        JsonVariantConst v = root[key];
        if (v.isNull())
        {
            error = missing(key);
            return nullptr;
        }
        if (!v.is<const char *>())
        {
            error = bad(key);
            return nullptr;
        }
        const char *s = v.as<const char *>();
        if (!valid(s))
        {
            error = bad(key);
            return nullptr;
        }
        return s;
    }

    // Kolom bilangan bulat tak negatif. Kolom opsional yang tidak ada dibiarkan memakai nilai awal `value`.
    bool readUint(JsonObjectConst root, const char *key, bool required, uint32_t minValue,
                  uint32_t maxValue, uint32_t &value, ManifestResult &error)
    {
        JsonVariantConst v = root[key];
        if (v.isNull())
        {
            if (required)
            {
                error = missing(key);
                return false;
            }
            return true;
        }
        if (!v.is<uint32_t>())
        {
            error = bad(key);
            return false;
        }
        const uint32_t x = v.as<uint32_t>();
        if (x < minValue || x > maxValue)
        {
            error = bad(key);
            return false;
        }
        value = x;
        return true;
    }

    bool readOptionalBool(JsonObjectConst root, const char *key, bool &value, ManifestResult &error)
    {
        JsonVariantConst v = root[key];
        if (v.isNull())
            return true;
        if (!v.is<bool>())
        {
            error = bad(key);
            return false;
        }
        value = v.as<bool>();
        return true;
    }

} // namespace

const char *manifestErrorText(ManifestError error)
{
    switch (error)
    {
    case ManifestError::None:
        return "ok";
    case ManifestError::EmptyOrTooLarge:
        return "manifest kosong atau terlalu besar";
    case ManifestError::InvalidJson:
        return "JSON rusak";
    case ManifestError::UnsupportedSchema:
        return "skema manifest tidak didukung";
    case ManifestError::MissingField:
        return "kolom wajib tidak ada";
    case ManifestError::BadField:
        return "nilai kolom tidak valid";
    }
    return "tidak diketahui";
}

namespace ManifestParser
{

    ManifestResult parse(const char *json, size_t length, Manifest &out)
    {
        if (json == nullptr || length == 0 || length > ManifestLimits::MAX_BYTES)
        {
            return ManifestResult{ManifestError::EmptyOrTooLarge, ""};
        }

        JsonDocument doc;
        if (deserializeJson(doc, json, length) || !doc.is<JsonObject>())
        {
            return ManifestResult{ManifestError::InvalidJson, ""};
        }
        const JsonObjectConst root = doc.as<JsonObjectConst>();

        Manifest m;
        memset(&m, 0, sizeof(m));
        ManifestResult error{ManifestError::None, ""};

        if (!readUint(root, "schema", true, 0, 0xFFFFFFFFu, m.schema, error))
            return error;
        if (m.schema != ManifestLimits::SUPPORTED_SCHEMA)
        {
            return ManifestResult{ManifestError::UnsupportedSchema, "schema"};
        }

        const char *text = readText(root, "device_type", validType, error);
        if (text == nullptr)
            return error;
        snprintf(m.deviceType, sizeof(m.deviceType), "%s", text);

        text = readText(root, "board", validType, error);
        if (text == nullptr)
            return error;
        snprintf(m.board, sizeof(m.board), "%s", text);

        text = readText(root, "version", validVersion, error);
        if (text == nullptr)
            return error;
        snprintf(m.version, sizeof(m.version), "%s", text);

        if (!readUint(root, "build", true, 1, 0xFFFFFFFFu, m.build, error))
            return error;
        if (!readUint(root, "size", true, 1, ManifestLimits::MAX_FIRMWARE_BYTES, m.size, error))
            return error;

        text = readText(root, "sha256", validSha256, error);
        if (text == nullptr)
            return error;
        decodeSha256(text, m.sha256);

        if (!readUint(root, "min_from_build", false, 0, 0xFFFFFFFFu, m.minFromBuild, error))
            return error;
        m.allowDowngrade = false;
        if (!readOptionalBool(root, "allow_downgrade", m.allowDowngrade, error))
            return error;

        out = m;
        return ManifestResult{ManifestError::None, ""};
    }

} // namespace ManifestParser