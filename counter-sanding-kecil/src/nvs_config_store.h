#pragma once
#include <Preferences.h>
#include <string.h>
#include "iconfig_store.h"

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