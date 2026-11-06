#pragma once
#include <PubSubClient.h>
#include "sanding_config.h"
#include "imqtt_client.h"

class PubSubMqttClient : public IMqttClient
{
public:
    PubSubMqttClient(Client &network, const char *host, uint16_t port) : client_(network)
    {
        client_.setSocketTimeout(config::MQTT_SOCKET_TIMEOUT_S);
    }

    void setServer(const char *host, uint16_t port) { client_.setServer(host, port); }

    bool connected() override { return client_.connected(); }
    bool connect(const MqttConnectParams &p) override
    {
        return client_.connect(p.clientId, p.user, p.pass, p.willTopic, 0, p.willRetain, p.willMessage);
    }
    int state() override { return client_.state(); }
    bool subscribe(const char *topic) override { return client_.subscribe(topic); }
    bool publish(const char *topic, const char *payload, bool retain) override
    {
        return client_.publish(topic, payload, retain);
    }
    bool loop() override { return client_.loop(); }
    void disconnect() override { client_.disconnect(); }

    void onMessage(MessageCallback callback) override
    {
        callback_ = callback;
        client_.setCallback([this](char *topic, uint8_t *payload, unsigned int length)
                            {
      if (callback_) callback_(topic, payload, length); });
    }

private:
    PubSubClient client_;
    MessageCallback callback_;
};