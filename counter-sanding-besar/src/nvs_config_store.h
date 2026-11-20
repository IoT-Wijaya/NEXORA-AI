#pragma once
#include <Preferences.h>
#include <string.h>
#include "../lib/device_config/iconfig_store.h"

// Penyimpanan konfigurasi di NVS/Preferences ESP32 (implementasi IConfigStore).
// Namespace "sanding" dipisahkan dari namespace counter/OTA lainnya.
class NvsConfigStore : public IConfigStore
{
public:
    bool begin() { return prefs_.begin("sanding", false); }

    bool getString(const char *key, char *out, size_t size) override
    {
        if (!prefs_.isKey(key))
            return false;
        prefs_.getString(key, out, size);
        return true;
    }
    bool setString(const char *key, const char *value) override
    {
        return prefs_.putString(key, value) == strlen(value);
    }
    bool erase(const char *key) override { return !prefs_.isKey(key) || prefs_.remove(key); }
    bool eraseAll() override { return prefs_.clear(); }

private:
    Preferences prefs_;
};
