#pragma once
#include <SPI.h>
#include <string.h>
#include <Ethernet.h>
#include "sanding_config.h"
#include "lib/device_identity.h"
#include "lib/net_link/inet_driver.h"

// Driver W5500 Ethernet controller melalui library Arduino Ethernet
// (implementasi INetDriver). MAC address diambil dari DeviceIdentity.
class W5500Driver : public INetDriver
{
public:
    explicit W5500Driver(const DeviceIdentity &identity) : identity_(identity) {}

    void begin() override
    {
        SPI.begin(config::W5500_SCK_PIN, config::W5500_MISO_PIN, config::W5500_MOSI_PIN,
                  config::W5500_CS_PIN);
        Ethernet.init(config::W5500_CS_PIN);
    }
    bool linkUp() override { return Ethernet.linkStatus() == LinkON; }
    bool hasIP() override { return (uint32_t)Ethernet.localIP() != 0; }
    bool tryDhcp() override
    {
        uint8_t mac[6]; // Ethernet.begin() meminta pointer yang tidak const
        memcpy(mac, identity_.mac(), sizeof(mac));
        return Ethernet.begin(mac, config::DHCP_TIMEOUT_MS, config::DHCP_RESPONSE_TIMEOUT_MS) != 0;
    }
    void maintain() override { Ethernet.maintain(); }
    Ip4 localIP() override
    {
        const IPAddress ip = Ethernet.localIP();
        return Ip4{{ip[0], ip[1], ip[2], ip[3]}};
    }

private:
    const DeviceIdentity &identity_;
};
