#include "device_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

    const char *const MSG_HOST = "Host harus 1-63 karakter: huruf, angka, titik, atau strip.";
    const char *const MSG_PORT = "Port harus angka 1-65535.";
    const char *const MSG_USER = "User harus 1-31 karakter tanpa spasi.";
    const char *const MSG_PASS = "Kata sandi harus 1-63 karakter ASCII yang tercetak.";
    const char *const MSG_NAME = "Nama mesin harus 1-10 karakter ASCII tercetak, tanpa spasi di ujung.";
    const char *const MSG_UNKNOWN = "Kunci tidak dikenal. Ketik 'help' untuk daftar kunci.";
    const char *const MSG_WRITE = "Gagal menulis ke penyimpanan.";

    bool isAlnum(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }
    bool isGraph(char c) { return c > 0x20 && c <= 0x7E; }      // tercetak, bukan spasi
    bool isPrintable(char c) { return c >= 0x20 && c <= 0x7E; } // tercetak, termasuk spasi

    const char *validateHost(const char *v)
    {
        const size_t n = strlen(v);
        if (n < 1 || n > ConfigLimits::MAX_HOST_LEN)
            return MSG_HOST;
        for (size_t i = 0; i < n; i++)
        {
            if (!isAlnum(v[i]) && v[i] != '.' && v[i] != '-')
                return MSG_HOST;
        }
        return nullptr;
    }

    const char *validatePort(const char *v)
    {
        const size_t n = strlen(v);
        if (n < 1 || n > 5)
            return MSG_PORT;
        unsigned long x = 0;
        for (size_t i = 0; i < n; i++)
        {
            if (v[i] < '0' || v[i] > '9')
                return MSG_PORT;
            x = x * 10 + (unsigned long)(v[i] - '0');
        }
        if (x < 1 || x > 65535)
            return MSG_PORT;
        return nullptr;
    }

    const char *validateUser(const char *v)
    {
        const size_t n = strlen(v);
        if (n < 1 || n > ConfigLimits::MAX_USER_LEN)
            return MSG_USER;
        for (size_t i = 0; i < n; i++)
        {
            if (!isGraph(v[i]))
                return MSG_USER;
        }
        return nullptr;
    }

    const char *validatePass(const char *v)
    {
        const size_t n = strlen(v);
        if (n < 1 || n > ConfigLimits::MAX_PASS_LEN)
            return MSG_PASS;
        for (size_t i = 0; i < n; i++)
        {
            if (!isPrintable(v[i]))
                return MSG_PASS;
        }
        return nullptr;
    }

    const char *validateName(const char *v)
    {
        const size_t n = strlen(v);
        if (n < 1 || n > ConfigLimits::MAX_NAME_LEN)
            return MSG_NAME;
        if (v[0] == ' ' || v[n - 1] == ' ')
            return MSG_NAME;
        for (size_t i = 0; i < n; i++)
        {
            if (!isPrintable(v[i]))
                return MSG_NAME;
        }
        return nullptr;
    }

} // namespace

ConfigManager::ConfigManager(IConfigStore &store, const char *defaultMachineName, uint16_t defaultPort)
    : store_(store), defaultName_(defaultMachineName), defaultPort_(defaultPort) {}

bool ConfigManager::isKnownKey(const char *key)
{
    return key != nullptr &&
           (strcmp(key, ConfigKeys::MQTT_HOST) == 0 || strcmp(key, ConfigKeys::MQTT_PORT) == 0 ||
            strcmp(key, ConfigKeys::MQTT_USER) == 0 || strcmp(key, ConfigKeys::MQTT_PASS) == 0 ||
            strcmp(key, ConfigKeys::MACHINE_NAME) == 0);
}

const char *ConfigManager::validate(const char *key, const char *value)
{
    if (!isKnownKey(key) || value == nullptr)
        return MSG_UNKNOWN;
    if (strcmp(key, ConfigKeys::MQTT_HOST) == 0)
        return validateHost(value);
    if (strcmp(key, ConfigKeys::MQTT_PORT) == 0)
        return validatePort(value);
    if (strcmp(key, ConfigKeys::MQTT_USER) == 0)
        return validateUser(value);
    if (strcmp(key, ConfigKeys::MQTT_PASS) == 0)
        return validatePass(value);
    return validateName(value);
}

bool ConfigManager::readValid(const char *key, char *buffer, size_t size, int &invalidCount) const
{
    if (!store_.getString(key, buffer, size))
        return false;
    if (validate(key, buffer) != nullptr)
    {
        invalidCount++;
        return false;
    }
    return true;
}

int ConfigManager::load(DeviceConfig &out) const
{
    memset(&out, 0, sizeof(out));
    out.mqttPort = defaultPort_;
    snprintf(out.machineName, sizeof(out.machineName), "%s", defaultName_);

    int invalid = 0;
    char buf[96];

    if (readValid(ConfigKeys::MQTT_HOST, buf, sizeof(buf), invalid))
        snprintf(out.mqttHost, sizeof(out.mqttHost), "%s", buf);
    if (readValid(ConfigKeys::MQTT_PORT, buf, sizeof(buf), invalid))
        out.mqttPort = (uint16_t)strtoul(buf, nullptr, 10);
    if (readValid(ConfigKeys::MQTT_USER, buf, sizeof(buf), invalid))
        snprintf(out.mqttUser, sizeof(out.mqttUser), "%s", buf);
    if (readValid(ConfigKeys::MQTT_PASS, buf, sizeof(buf), invalid))
        snprintf(out.mqttPass, sizeof(out.mqttPass), "%s", buf);
    if (readValid(ConfigKeys::MACHINE_NAME, buf, sizeof(buf), invalid))
        snprintf(out.machineName, sizeof(out.machineName), "%s", buf);

    return invalid;
}

const char *ConfigManager::set(const char *key, const char *value)
{
    const char *error = validate(key, value);
    if (error != nullptr)
        return error;
    if (!store_.setString(key, value))
        return MSG_WRITE;
    return nullptr;
}

const char *ConfigManager::unset(const char *key)
{
    if (!isKnownKey(key))
        return MSG_UNKNOWN;
    if (!store_.erase(key))
        return MSG_WRITE;
    return nullptr;
}

bool ConfigManager::clearAll() { return store_.eraseAll(); }