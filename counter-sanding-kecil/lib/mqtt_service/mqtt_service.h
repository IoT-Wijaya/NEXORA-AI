#pragma once
#include <functional>
#include <stdint.h>
#include "command_parser.h"
#include "imqtt_client.h"

struct MqttSettings
{
    char clientId[32];
    const char *user; // nullptr atau "" = tanpa autentikasi
    const char *pass;
    bool useLastWill;
    bool enabled;
};

enum class MqttEvent
{
    None,
    Connected,
    ConnectFailed,
};

// Mengelola koneksi MQTT: sambung ulang, heartbeat, publish, dan perintah masuk.
// Menggantikan connectMQTT, mqttOK, lastMQTTReconnect, lastHeartbeat, dan semua
// fungsi publish yang tadinya tersebar di main.cpp.
class MqttService
{
public:
    using CommandHandler = std::function<void(Command)>;

    explicit MqttService(IMqttClient &client);

    void begin(const MqttSettings &settings);
    void onCommand(CommandHandler handler);

    // Dipanggil berkala dari networkTask.
    MqttEvent update(bool networkOnline, uint32_t nowMs);

    bool isConnected();
    int lastState() const { return lastState_; }

    // Dipanggil pemanggil saat update() mengembalikan Connected.
    // Urutan: jumlah, jumlah2, mode, ip (semuanya retained).
    void publishOnConnect(uint32_t hasil, uint32_t modal, bool aktif, const char *ip);

    // Dipanggil saat counter berubah. Angka SELALU dikirim lebih dulu; jika ada
    // ack, mode dan ack menyusul, sehingga HMI yang menerima ack pasti sudah
    // menerima angkanya.
    void publishChange(uint32_t hasil, uint32_t modal, bool aktif, const char *ack);

    // Versi firmware sebagai JSON, retained. Dipanggil sekali setiap MQTT tersambung.
    void publishVersion(const char *json);

private:
    MqttEvent tryConnect(uint32_t nowMs);
    void publishNumber(const char *topic, uint32_t value);
    void publishMode(bool aktif);

    IMqttClient &client_;
    MqttSettings settings_;
    CommandHandler handler_;
    bool attempted_;
    uint32_t lastAttemptMs_;
    uint32_t lastHeartbeatMs_;
    int lastState_;
};