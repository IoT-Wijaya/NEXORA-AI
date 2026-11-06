#pragma once
#include <Arduino.h>
#include <esp_system.h>
#if __has_include(<esp_mac.h>)
#include <esp_mac.h>
#endif
#include "imac_source.h"

// MAC Ethernet turunan dari eFuse. Unik per chip dan memang dicadangkan
// Espressif untuk Ethernet, jadi tidak bentrok dengan MAC WiFi atau Bluetooth.
class EspMacSource : public IMacSource
{
public:
    bool readMac(uint8_t out[6]) override
    {
        return esp_read_mac(out, ESP_MAC_ETH) == ESP_OK;
    }
};