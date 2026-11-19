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
      lastAttemptMs_(0), retryDelay_(config::MQTT_RECONNECT_INTERVAL),
      lastHeartbeatMs_(0), nowMs_(0), connectedAtMs_(0), lastState_(0) {}

void MqttService::begin(const MqttSettings &settings)
{
    settings_ = settings;
    client_.onMessage([this](const char *topic, const uint8_t *payload, size_t length)
                      { onMessage(topic, payload, length); });
}

void MqttService::onCommand(CommandHandler handler) { handler_ = handler; }

void MqttService::onMessage(const char *topic, const uint8_t *payload, size_t length)
{
    if (topic != nullptr && strcmp(topic, config::topicPerintah) == 0)
    {
        handler_(CommandParser::parse((const char *)payload, length));
    }
}

bool MqttService::isConnected() { return client_.connected(); }

MqttEvent MqttService::update(bool networkOnline, uint32_t nowMs)
{
    if (!settings_.enabled)
        return MqttEvent::None;

    nowMs_ = nowMs;

    if (!networkOnline)
    {
        // Jaringan putus: tutup koneksi lama agar tidak menggantung.
        if (client_.connected())
            client_.disconnect();
        return MqttEvent::None;
    }

    if (!client_.connected())
    {
        // Jeda mengikuti backoff (2 detik -> ... -> 60 detik).
        if (attempted_ && (nowMs - lastAttemptMs_) < retryDelay_)
            return MqttEvent::None;
        attempted_ = true;
        lastAttemptMs_ = nowMs;
        MqttEvent hasil = tryConnect(nowMs);
        if (hasil == MqttEvent::ConnectFailed)
        {
            // Gagal: jeda berikutnya digandakan, maksimal MQTT_RECONNECT_MAX.
            retryDelay_ = (retryDelay_ < config::MQTT_RECONNECT_MAX / 2)
                              ? retryDelay_ * 2
                              : config::MQTT_RECONNECT_MAX;
        }
        else
        {
            retryDelay_ = config::MQTT_RECONNECT_INTERVAL; // berhasil: jeda dikembalikan ke awal
        }
        return hasil;
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
    connectedAtMs_ = nowMs;
    return MqttEvent::Connected;
}

bool MqttService::publishOnConnect(uint32_t hasil, uint32_t modal, bool aktif, const char *ip)
{
    // Semua publish tetap dicoba berurutan; hasilnya digabung (bukan berhenti di yang gagal),
    // supaya urutan kirim tidak berubah dan satu kegagalan tidak menahan yang lain.
    bool semuaDiterima = true;
    semuaDiterima &= publishNumber(config::topicJumlah, hasil);
    semuaDiterima &= publishNumber(config::topicJumlah2, modal);
    semuaDiterima &= publishMode(aktif);
    if (ip != nullptr)
        semuaDiterima &= client_.publish(config::topicIP, ip, true);
    return semuaDiterima;
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

bool MqttService::publishNumber(const char *topic, uint32_t value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    return client_.publish(topic, buffer, true);
}

bool MqttService::publishMode(bool aktif)
{
    return client_.publish(config::topicMode, aktif ? "ON" : "OFF", true);
}

bool MqttService::publishDiag(const char *json)
{
    return client_.publish(config::topicDiag, json, true);
}
