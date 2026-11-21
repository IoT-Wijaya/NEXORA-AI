#pragma once
#include "sanding_config.h" // harus pertama: mendefinisikan ETH_HEADER
#include <SPI.h>
#include <string.h>
#include ETH_HEADER // dipilih per mesin: EthernetENC (ENC28J60) atau Ethernet (W5500)
#include "device_identity.h"
#include "inet_driver.h"

// Driver Ethernet untuk ENC28J60 maupun W5500: kedua pustaka memakai API yang sama,
// jadi bedanya hanya header di ETH_HEADER dan pin di config.
class EthDriver : public INetDriver
{
public:
    explicit EthDriver(const DeviceIdentity &identity) : identity_(identity) {}

    void begin() override
    {
        SPI.begin(config::ETH_SCK, config::ETH_MISO, config::ETH_MOSI, config::ETH_CS_PIN);
        Ethernet.init(config::ETH_CS_PIN);
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