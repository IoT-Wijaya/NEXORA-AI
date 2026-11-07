#include "sanding_config.h"
#include "mqtt_service.h"
#include <stdio.h>
#include <string.h>

namespace
{
    constexpr const char *PAYLOAD_ONLINE = "ONLINE";
    constexpr const char *PAYLOAD_OFFLINE = "OFFLINE";
} // namespace

MqttService::MqttService(IMqttClient &client)
    : client_(client), settings_{}, attempted_(false),
      lastAttemptMs_(0), lastHeartbeatMs_(0), lastState_(0) {}

void MqttService::begin(const MqttSettings &settings)
{
    settings_ = settings;
    client_.onMessage([this](const char *, const uint8_t *payload, size_t length)
                      {
    if (handler_) handler_(CommandParser::parse((const char *)payload, length)); });
}

void MqttService::onCommand(CommandHandler handler) { handler_ = handler; }

bool MqttService::isConnected() { return client_.connected(); }

MqttEvent MqttService::update(bool networkOnline, uint32_t nowMs)
{
    if (!settings_.enabled)
        return MqttEvent::None;

    if (!networkOnline)
    {
        // Jaringan putus: tutup koneksi lama agar tidak menggantung.
        if (client_.connected())
            client_.disconnect();
        return MqttEvent::None;
    }

    if (!client_.connected())
    {
        if (attempted_ && (nowMs - lastAttemptMs_) < config::MQTT_RECONNECT_INTERVAL)
        {
            return MqttEvent::None;
        }
        attempted_ = true;
        lastAttemptMs_ = nowMs;
        return tryConnect(nowMs);
    }

    // loop() mengembalikan false jika koneksi ternyata sudah putus.
    if (client_.loop() && (nowMs - lastHeartbeatMs_) >= config::HEARTBEAT_INTERVAL)
    {
        lastHeartbeatMs_ = nowMs;
        client_.publish(config::topicStatus, PAYLOAD_ONLINE, true);
    }
    return MqttEvent::None;
}

MqttEvent MqttService::tryConnect(uint32_t nowMs)
{
    const bool adaUser = settings_.user != nullptr && settings_.user[0] != '\0';

    MqttConnectParams p;
    p.clientId = settings_.clientId;
    p.user = adaUser ? settings_.user : nullptr;
    p.pass = adaUser ? settings_.pass : nullptr;
    if (settings_.useLastWill)
    {
        p.willTopic = config::topicStatus;
        p.willMessage = PAYLOAD_OFFLINE;
        p.willRetain = true;
    }

    if (!client_.connect(p))
    {
        lastState_ = client_.state();
        return MqttEvent::ConnectFailed;
    }

    client_.subscribe(config::topicPerintah);
    client_.publish(config::topicStatus, PAYLOAD_ONLINE, true);
    lastHeartbeatMs_ = nowMs;
    return MqttEvent::Connected;
}

void MqttService::publishOnConnect(uint32_t hasil, uint32_t modal, bool aktif, const char *ip)
{
    publishNumber(config::topicJumlah, hasil);
    publishNumber(config::topicJumlah2, modal);
    publishMode(aktif);
    if (ip != nullptr)
        client_.publish(config::topicIP, ip, true);
}

void MqttService::publishChange(uint32_t hasil, uint32_t modal, bool aktif, const char *ack)
{
    publishNumber(config::topicJumlah, hasil);
    publishNumber(config::topicJumlah2, modal);
    if (ack != nullptr)
    {
        publishMode(aktif);
        client_.publish(config::topicAck, ack, false);
    }
}

void MqttService::publishNumber(const char *topic, uint32_t value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    client_.publish(topic, buffer, true);
}

void MqttService::publishMode(bool aktif)
{
    client_.publish(config::topicMode, aktif ? "ON" : "OFF", true);
}

void MqttService::publishVersion(const char *json)
{
    client_.publish(config::topicVersi, json, true);
}