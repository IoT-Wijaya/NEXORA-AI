#pragma once
#include <SPI.h>
#include <string.h>
#include <EthernetENC.h>
#include "sanding_config.h"
#include "device_identity.h"
#include "inet_driver.h"

class EncDriver : public INetDriver
{
public:
    explicit EncDriver(const DeviceIdentity &identity) : identity_(identity) {}

    void begin() override
    {
        SPI.begin(config::ENC_SCK, config::ENC_MISO, config::ENC_MOSI, config::ENC_CS_PIN);
        Ethernet.init(config::ENC_CS_PIN);
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