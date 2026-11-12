#pragma once
#include <functional>
#include <stdint.h>
#include "command_parser.h"
#include "ota_command.h"
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
// Nasib sebuah pesan di topik OTA.
enum class OtaMessage
{
    Valid,     // "ota <build>" sah, build terisi
    Malformed, // bukan perintah OTA yang sah
    TooEarly,  // tiba terlalu dekat dengan saat tersambung (mungkin pesan retained lama)
};

class MqttService
{
public:
    using CommandHandler = std::function<void(Command)>;
    using OtaHandler = std::function<void(OtaMessage, uint32_t build)>;

    explicit MqttService(IMqttClient &client);

    void begin(const MqttSettings &settings);
    void onCommand(CommandHandler handler);

    // Pesan di config::topicOta. Hanya OtaMessage::Valid yang membawa build.
    // Pesan yang tiba dalam config::MQTT_OTA_HOLDOFF_MS sejak tersambung dilaporkan TooEarly:
    // broker mengirim pesan retained tepat setelah subscribe, dan perintah OTA lama tidak
    // boleh terlaksana ulang pada setiap sambung ulang.
    void onOtaCommand(OtaHandler handler);

    // Dipanggil berkala dari networkTask.
    MqttEvent update(bool networkOnline, uint32_t nowMs);

    bool isConnected();
    int lastState() const { return lastState_; }

    // Dipanggil pemanggil saat update() mengembalikan Connected.
    // Urutan: jumlah, jumlah2, mode, ip (semuanya retained).
    // Mengembalikan true bila SEMUA publish diterima klien MQTT.
    bool publishOnConnect(uint32_t hasil, uint32_t modal, bool aktif, const char *ip);

    // Dipanggil saat counter berubah. Angka SELALU dikirim lebih dulu; jika ada
    // ack, mode dan ack menyusul, sehingga HMI yang menerima ack pasti sudah
    // menerima angkanya.
    void publishChange(uint32_t hasil, uint32_t modal, bool aktif, const char *ack);

    // Versi firmware sebagai JSON, retained. Dipanggil sekali setiap MQTT tersambung.
    // Mengembalikan true bila publish diterima klien MQTT.
    bool publishVersion(const char *json);

    // Hasil OTA terakhir sebagai JSON, retained, ke config::topicOtaStatus.
    // Mengembalikan true bila publish diterima klien MQTT.
    bool publishOtaReport(const char *json);

private:
    MqttEvent tryConnect(uint32_t nowMs);
    void onMessage(const char *topic, const uint8_t *payload, size_t length);
    void onOtaMessage(const uint8_t *payload, size_t length);
    bool publishNumber(const char *topic, uint32_t value);
    bool publishMode(bool aktif);

    IMqttClient &client_;
    MqttSettings settings_;
    CommandHandler handler_;
    OtaHandler otaHandler_;
    bool attempted_;
    uint32_t lastAttemptMs_;
    uint32_t lastHeartbeatMs_;
    uint32_t nowMs_;         // waktu update() terakhir; dipakai callback yang berjalan di dalam loop()
    uint32_t connectedAtMs_; // waktu sambung terakhir yang berhasil
    int lastState_;
};