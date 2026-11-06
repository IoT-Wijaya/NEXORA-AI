#pragma once
#include <functional>
#include <stddef.h>
#include <stdint.h>

struct MqttConnectParams
{
    const char *clientId = nullptr;
    const char *user = nullptr; // nullptr = tanpa autentikasi
    const char *pass = nullptr;
    const char *willTopic = nullptr; // nullptr = tanpa Last Will
    const char *willMessage = nullptr;
    bool willRetain = false;
};

using MessageCallback = std::function<void(const char *topic, const uint8_t *payload, size_t length)>;

// Antarmuka klien MQTT. Di perangkat diisi PubSubClient, di komputer (test) diisi pengganti.
class IMqttClient
{
public:
    virtual ~IMqttClient() = default;
    virtual bool connected() = 0;
    virtual bool connect(const MqttConnectParams &params) = 0;
    virtual int state() = 0;
    virtual bool subscribe(const char *topic) = 0;
    virtual bool publish(const char *topic, const char *payload, bool retain) = 0;
    virtual bool loop() = 0;
    virtual void disconnect() = 0;
    virtual void onMessage(MessageCallback callback) = 0;
};