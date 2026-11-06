#pragma once
#include <stddef.h>
#include <stdint.h>
#include "iconfig_store.h"

// Nama kunci di NVS. Maksimal 15 karakter (batas NVS).
namespace ConfigKeys
{
    constexpr const char *MQTT_HOST = "mqtt_host";
    constexpr const char *MQTT_PORT = "mqtt_port";
    constexpr const char *MQTT_USER = "mqtt_user";
    constexpr const char *MQTT_PASS = "mqtt_pass";
    constexpr const char *MACHINE_NAME = "mach_name";
} // namespace ConfigKeys

namespace ConfigLimits
{
    constexpr size_t MAX_HOST_LEN = 63;
    constexpr size_t MAX_USER_LEN = 31;
    constexpr size_t MAX_PASS_LEN = 63;
    constexpr size_t MAX_NAME_LEN = 10; // "MESIN:" + 10 karakter = 16 kolom LCD
} // namespace ConfigLimits

// Konfigurasi yang dipakai perangkat. Diisi sekali saat boot, sesudahnya hanya dibaca.
struct DeviceConfig
{
    char mqttHost[ConfigLimits::MAX_HOST_LEN + 1];
    uint16_t mqttPort;
    char mqttUser[ConfigLimits::MAX_USER_LEN + 1];
    char mqttPass[ConfigLimits::MAX_PASS_LEN + 1];
    char machineName[ConfigLimits::MAX_NAME_LEN + 1];
};

class ConfigManager
{
public:
    ConfigManager(IConfigStore &store, const char *defaultMachineName, uint16_t defaultPort);

    // Membaca semua nilai. Nilai yang belum ada atau tidak valid diganti nilai bawaan.
    // Mengembalikan jumlah nilai tersimpan yang tidak valid.
    int load(DeviceConfig &out) const;

    // Memeriksa lalu menyimpan satu nilai. Mengembalikan nullptr jika berhasil,
    // atau pesan galat. Penyimpanan tidak berubah jika galat.
    const char *set(const char *key, const char *value);

    // Menghapus satu nilai sehingga kembali ke bawaan.
    const char *unset(const char *key);

    bool clearAll();

    // nullptr jika valid, selain itu pesan galat.
    static const char *validate(const char *key, const char *value);
    static bool isKnownKey(const char *key);

    // Cukup lengkap untuk menyambung MQTT: alamat broker sudah diisi.
    static bool isComplete(const DeviceConfig &config) { return config.mqttHost[0] != '\0'; }

private:
    bool readValid(const char *key, char *buffer, size_t size, int &invalidCount) const;

    IConfigStore &store_;
    const char *defaultName_;
    uint16_t defaultPort_;
};