#pragma once
#include <stddef.h>
#include <stdint.h>
#include "imac_source.h"

namespace MacFormat
{
    constexpr size_t HEX_SIZE = 13;   // "240AC4123456" + NUL
    constexpr size_t COLON_SIZE = 18; // "24:0A:C4:12:34:56" + NUL

    // MAC dianggap valid jika bukan nol semua dan bukan multicast.
    bool isValid(const uint8_t mac[6]);
    void toHex(const uint8_t mac[6], char *out, size_t size);
    void toColon(const uint8_t mac[6], char *out, size_t size);
} // namespace MacFormat

// Identitas perangkat: MAC dan Client ID MQTT, keduanya diturunkan dari chip.
// Satu biner firmware aman dipakai banyak mesin karena tidak ada yang ditanam di kode.
class DeviceIdentity
{
public:
    static constexpr size_t CLIENT_ID_SIZE = 32;

    DeviceIdentity(IMacSource &source, const char *clientIdPrefix);

    // Membaca MAC dan menyusun teks. true jika MAC berhasil dibaca dan valid.
    // Jika gagal, clientId() berakhiran "INVALID" supaya tetap ada nilai yang jelas.
    bool begin();

    const uint8_t *mac() const { return mac_; }
    const char *macText() const { return macText_; }   // "24:0A:C4:12:34:56"
    const char *clientId() const { return clientId_; } // "<prefix><12 digit hex>"
    bool isValid() const { return valid_; }

private:
    IMacSource &source_;
    const char *prefix_;
    uint8_t mac_[6];
    char macText_[MacFormat::COLON_SIZE];
    char clientId_[CLIENT_ID_SIZE];
    bool valid_;
};