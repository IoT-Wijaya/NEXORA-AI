#pragma once
#include <stddef.h>

// Penyimpanan pasangan kunci-nilai. Di perangkat diisi NVS, di komputer (test) diisi pengganti.
class IConfigStore
{
public:
    virtual ~IConfigStore() = default;
    virtual bool getString(const char *key, char *out, size_t size) = 0; // false jika kunci tidak ada
    virtual bool setString(const char *key, const char *value) = 0;
    virtual bool erase(const char *key) = 0;
    virtual bool eraseAll() = 0;
};