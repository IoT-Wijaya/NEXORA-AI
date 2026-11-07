#pragma once
#include <Preferences.h>
#include "iblob_store.h"

class NvsBlobStore : public IBlobStore
{
public:
    explicit NvsBlobStore(const char *ns) : ns_(ns) {}

    bool begin() { return prefs_.begin(ns_, false); }

    int read(const char *key, uint8_t *buffer, size_t capacity) override
    {
        if (!prefs_.isKey(key))
            return -1;
        const size_t length = prefs_.getBytesLength(key);
        if (length == 0)
            return -1;
        if (length > capacity)
            return (int)length; // terlalu besar: pemanggil akan menolaknya
        return (int)prefs_.getBytes(key, buffer, capacity);
    }

    bool write(const char *key, const uint8_t *data, size_t length) override
    {
        return prefs_.putBytes(key, data, length) == length;
    }

private:
    const char *ns_;
    Preferences prefs_;
};