// diagnostik.h
// Di-include di main.cpp TEPAT SEBELUM blok "SETUP & LOOP" (setelah semua global
// dan fungsi OTA). Isi: log berlevel, statistik MQTT, publish aman, pengukur lag
// loop, perintah serial/MQTT, dan mode tes hardware (tombol + rotary encoder).
#pragma once
#ifdef __INTELLISENSE__
#include "main.cpp"
#endif
#include <stdarg.h>

// =====================================================
//  LOG BERLEVEL (ubah lewat serial "debug 0..3" atau MQTT DEBUG_0..DEBUG_3)
//  0 = mati, 1 = error, 2 = info (bawaan), 3 = debug (semua pesan MQTT masuk, dst.)
// =====================================================
enum LogLevel : uint8_t
{
    LV_OFF = 0,
    LV_ERR = 1,
    LV_INFO = 2,
    LV_DBG = 3
};
uint8_t logLevel = LV_INFO;

void hmiLog(uint8_t lvl, const char *tag, const char *fmt, ...)
{
    if (lvl > logLevel)
        return;
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    unsigned long s = millis() / 1000UL;
    Serial.printf("[%02lu:%02lu:%02lu][%-6s] %s\n", s / 3600UL, (s / 60UL) % 60UL, s % 60UL, tag, msg);
}

#define LOGE(tag, ...) hmiLog(LV_ERR, tag, __VA_ARGS__)
#define LOGI(tag, ...) hmiLog(LV_INFO, tag, __VA_ARGS__)
#define LOGD(tag, ...) hmiLog(LV_DBG, tag, __VA_ARGS__)

const char *topicHmiDiag2 = TOPIC_HMI "diag2";
const char *topicHmiHw = TOPIC_HMI "hw";
bool hwTestAktif = false; // dipakai publishDiag2(), diatur oleh blok tes hardware

// =====================================================
//  STATISTIK & LOG MQTT
// =====================================================
uint32_t mqttTryCount = 0, mqttTryOk = 0, mqttTryFail = 0, mqttPubFail = 0;
uint8_t mqttFailBeruntun = 0, mqttPubFailStreak = 0;
unsigned long mqttConnectedSince = 0;

const char *namaNs(int ns)
{
    return (ns == NS_ONLINE) ? "ONLINE" : namaNetState((NetState)ns);
}

void logMqttPercobaan(bool ok, int state, unsigned long durMs)
{
    mqttTryCount++;
    if (ok)
    {
        mqttTryOk++;
        LOGI("MQTT", "TERHUBUNG %lu ms (percobaan ke-%lu, gagal beruntun sebelumnya: %u)",
             durMs, (unsigned long)mqttTryCount, (unsigned)mqttFailBeruntun);
        mqttFailBeruntun = 0;
        mqttConnectedSince = millis();
    }
    else
    {
        mqttTryFail++;
        if (mqttFailBeruntun < 255)
            mqttFailBeruntun++;
        LOGE("MQTT", "GAGAL state=%d (%s) %lu ms, beruntun=%u, coba lagi %lu ms | link=%d ip=%s",
             state, namaErrorMQTT(state), durMs, (unsigned)mqttFailBeruntun, mqttRetryDelay,
             (int)linkUp, Ethernet.localIP().toString().c_str());
    }
}

void logMqttPutus(int state)
{
    unsigned long up = (mqttConnectedSince == 0) ? 0 : (millis() - mqttConnectedSince) / 1000UL;
    LOGE("MQTT", "PUTUS state=%d (%s) setelah tersambung %lu dtk | link=%d ip=%s",
         state, namaErrorMQTT(state), up, (int)linkUp, Ethernet.localIP().toString().c_str());
}

void logMqttMasuk(const char *topic, const byte *p, unsigned int len)
{
    if (logLevel < LV_DBG)
        return;
    char s[41];
    unsigned int n = (len > 40u) ? 40u : len;
    memcpy(s, p, n);
    s[n] = 0;
    LOGD("MQTT<", "%s = %s%s", topic, s, (len > 40u) ? "..." : "");
}

void logPerubahanNet()
{
    static int prev = -1;
    int ns = (int)hitungNetState();
    if (ns == prev)
        return;
    LOGI("NET", "%s -> %s (link=%d lan=%d mqtt=%d)",
         prev < 0 ? "BOOT" : namaNs(prev), namaNs(ns), (int)linkUp, (int)lanOK, (int)mqttOK);
    prev = ns;
}

// Jeda eksponensial + jitter 25% (mencegah banyak HMI menyerbu broker bersamaan).
// Saat mesin bekerja batas atas dipendekkan agar pulih lebih cepat.
unsigned long backoffBerikutnya(unsigned long sekarang)
{
    const unsigned long batas = statusIdle() ? MQTT_RETRY_MAX : 15000UL;
    unsigned long d = sekarang * 2;
    if (d > batas)
        d = batas;
    return d + (unsigned long)random(0, (long)(d / 4) + 1);
}

void paksaSambungUlangMqtt(const char *alasan)
{
    LOGI("MQTT", "sambung ulang dipaksa: %s", alasan);
    client.disconnect();
    ethClient.stop();
    mqttRetryDelay = MQTT_RETRY_MIN;
    lastMqttAttempt = 0; // reconnectMQTT() langsung mencoba pada putaran loop berikutnya
}

// Publish yang mendeteksi koneksi setengah mati (connected() masih true padahal
// paket tidak sampai). 3 kegagalan beruntun -> sambung ulang paksa.
// Pakai untuk pesan kecil seperti heartbeat. Publish gagal juga bila payload > buffer.
bool mqttPublishAman(const char *topic, const char *payload, bool retain)
{
    if (!client.connected())
        return false;
    if (client.publish(topic, payload, retain))
    {
        mqttPubFailStreak = 0;
        return true;
    }
    mqttPubFail++;
    mqttPubFailStreak++;
    LOGE("MQTT", "publish %s gagal (beruntun=%u)", topic, (unsigned)mqttPubFailStreak);
    if (mqttPubFailStreak >= 3)
    {
        mqttPubFailStreak = 0;
        paksaSambungUlangMqtt("publish gagal 3x beruntun");
    }
    return false;
}

// =====================================================
//  STATISTIK HTTP (di kode asli variabel ini tidak pernah diisi)
//  httpRequest() di main.cpp diganti namanya menjadi httpRequestMentah();
//  fungsi di bawah ini membungkusnya.
// =====================================================
void catatHttp(int status)
{
    lastHttpCode = status;
    if (status >= 200 && status < 300)
    {
        httpOkCount++;
        httpFailStreak = 0;
        lastHttpOkMs = millis();
    }
    else
    {
        httpFailCount++;
        if (httpFailStreak < 255)
            httpFailStreak++;
    }
    LOGD("HTTP", "status %d (ok=%lu gagal=%lu)", status, (unsigned long)httpOkCount, (unsigned long)httpFailCount);
}

HttpHasil httpRequest(const char *method, const String &path, const String &body,
                      const char *namaHeader, const String &nilaiHeader)
{
    unsigned long t0 = millis();
    HttpHasil r = httpRequestMentah(method, path, body, namaHeader, nilaiHeader);
    LOGD("HTTP", "%s %s -> %d (%lu ms)", method, path.c_str(), r.status, millis() - t0);
    catatHttp(r.status);
    return r;
}

// =====================================================
//  PENGUKUR LAG LOOP
//  Encoder dibaca dengan polling, jadi putaran yang terjadi saat loop tertahan
//  (HTTP, TLS, MQTT connect, delay) akan hilang. Angka ini menunjukkan seberapa buruk.
// =====================================================
unsigned long loopLagMax = 0;

void ukurLoop()
{
    static unsigned long prev = 0;
    unsigned long now = millis();
    if (prev != 0)
    {
        unsigned long d = now - prev;
        if (d > loopLagMax)
            loopLagMax = d;
        if (d >= 1000UL)
            LOGI("LAG", "loop tertahan %lu ms (state=%d): encoder tidak terbaca selama itu", d, (int)currentState);
    }
    prev = now;
}

// =====================================================
//  DIAG TAMBAHAN VIA MQTT (topik pabrik/hmi_sanding/diag2)
// =====================================================
bool diag2Segera = false;
unsigned long lastDiag2 = 0;

void publishDiag2()
{
    if (!client.connected())
        return;
    char b[360];
    int n = snprintf(b, sizeof(b),
                     "{\"mqtt_try\":%lu,\"mqtt_ok\":%lu,\"mqtt_fail\":%lu,\"pub_fail\":%lu,"
                     "\"retry_ms\":%lu,\"loop_max_ms\":%lu,"
                     "\"heap\":%lu,\"heap_min\":%lu,\"heap_blk\":%lu,"
                     "\"w5500\":%d,\"log\":%u,\"hwtest\":%d}",
                     (unsigned long)mqttTryCount, (unsigned long)mqttTryOk,
                     (unsigned long)mqttTryFail, (unsigned long)mqttPubFail,
                     mqttRetryDelay, loopLagMax,
                     (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
                     (unsigned long)ESP.getMaxAllocHeap(),
                     (int)Ethernet.hardwareStatus(), (unsigned)logLevel, (int)hwTestAktif);
    if (n >= (int)sizeof(b))
    {
        LOGE("DIAG", "diag2 terpotong (%d byte)", n);
        return;
    }
    if (!client.publish(topicHmiDiag2, b, true))
        LOGE("DIAG", "diag2 gagal terkirim (%d byte; cek client.setBufferSize)", n);
    loopLagMax = 0; // lag maksimum dihitung per interval publish
}

// =====================================================
//  TES HARDWARE: tombol OK, tombol BACK, rotary encoder, LCD/I2C, pin, W5500
//  Aktif lewat serial "hwtest" atau MQTT "HWTEST_ON" (hanya saat HMI idle).
//  Keluar: tahan BACK 3 detik, serial "hwtest off", MQTT "HWTEST_OFF", atau otomatis 2 menit.
//  Selama mode ini aksi tombol TIDAK diteruskan ke aplikasi.
// =====================================================
unsigned long hwTestSelesaiMs = 0;
const unsigned long HW_TEST_DURASI_MS = 120000UL;
const unsigned long HW_TAHAN_KELUAR_MS = 3000UL;

struct HwTombol
{
    const char *nama;
    uint8_t pin;
    bool raw, stabil;
    unsigned long edgeMs, tekanMs;
    uint16_t bounce, hitung;
};
HwTombol hwSw, hwBack;

int hwLastClk = HIGH;
unsigned long hwClkEdgeMs = 0;
long hwEncPos = 0;
uint16_t hwCw = 0, hwCcw = 0, hwEncBounce = 0;
bool hwEncDirty = false;

void hwPublish(const char *dev, const char *ev, long val)
{
    if (!client.connected())
        return;
    char b[96];
    snprintf(b, sizeof(b), "{\"dev\":\"%s\",\"ev\":\"%s\",\"val\":%ld,\"ms\":%lu}", dev, ev, val, millis());
    client.publish(topicHmiHw, b, false);
}

void hwInitTombol(HwTombol &t, const char *nama, uint8_t pin)
{
    t.nama = nama;
    t.pin = pin;
    t.raw = false;
    t.stabil = false;
    t.edgeMs = millis();
    t.tekanMs = 0;
    t.bounce = 0;
    t.hitung = 0;
}

void hwProsesTombol(HwTombol &t)
{
    unsigned long now = millis();
    bool low = (digitalRead(t.pin) == LOW);
    if (low != t.raw)
    {
        if (now - t.edgeMs < 10)
            t.bounce++; // tepi sinyal < 10 ms setelah tepi sebelumnya = bouncing
        t.raw = low;
        t.edgeMs = now;
    }
    if (t.raw != t.stabil && now - t.edgeMs >= 25)
    {
        t.stabil = t.raw;
        if (t.stabil)
        {
            t.hitung++;
            t.tekanMs = now;
            Serial.printf("[HW] %-4s TEKAN  #%u (bounce total %u)\n", t.nama, (unsigned)t.hitung, (unsigned)t.bounce);
            hwPublish(t.nama, "tekan", t.hitung);
        }
        else
        {
            unsigned long dur = now - t.tekanMs;
            Serial.printf("[HW] %-4s LEPAS  setelah %lu ms\n", t.nama, dur);
            hwPublish(t.nama, "lepas", (long)dur);
        }
    }
}

// Logika sama dengan aplikasi: hitung hanya pada tepi turun CLK, arah dari DT
void hwProsesEncoder()
{
    int clk = digitalRead(PIN_CLK);
    if (clk == hwLastClk)
        return;
    unsigned long now = millis();
    if (now - hwClkEdgeMs < 2)
        hwEncBounce++;
    hwClkEdgeMs = now;
    hwLastClk = clk;
    if (clk != LOW)
        return;
    bool cw = (digitalRead(PIN_DT) == HIGH);
    if (cw)
    {
        hwEncPos++;
        hwCw++;
    }
    else
    {
        hwEncPos--;
        hwCcw++;
    }
    hwEncDirty = true;
    Serial.printf("[HW] ENC  %-3s pos=%ld (CW=%u CCW=%u bounce=%u)\n", cw ? "CW" : "CCW", hwEncPos,
                  (unsigned)hwCw, (unsigned)hwCcw, (unsigned)hwEncBounce);
}

void hwCekPin(const char *nama, uint8_t pin)
{
    uint8_t low = 0;
    for (int i = 0; i < 20; i++)
    {
        if (digitalRead(pin) == LOW)
            low++;
        delay(1);
    }
    const char *ket = (low == 0)    ? "HIGH (idle normal)"
                      : (low == 20) ? "LOW terus (ditekan / posisi encoder / korslet ke GND)"
                                    : "BERUBAH-UBAH (pin mengambang atau noise)";
    Serial.printf("[HW] pin %-5s GPIO%-2u : %s\n", nama, (unsigned)pin, ket);
}

void cekSemuaPin()
{
    hwCekPin("CLK", PIN_CLK);
    hwCekPin("DT", PIN_DT);
    hwCekPin("OK", PIN_SW);
    hwCekPin("BACK", PIN_BTN_PIN4);
    if (PIN_CLK >= 34 || PIN_DT >= 34)
        Serial.println("[HW] CATATAN: GPIO34-39 hanya-input dan TIDAK punya pull-up internal; "
                       "INPUT_PULLUP tidak berefek. Pastikan ada pull-up eksternal (10k ke 3V3) "
                       "atau modul encoder sudah memilikinya.");
}

void scanI2C()
{
    bool lcdAda = false;
    for (uint8_t a = 0x03; a < 0x78; a++)
    {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0)
        {
            Serial.printf("[HW] I2C ditemukan: 0x%02X%s\n", a, a == 0x27 ? " (LCD)" : "");
            if (a == 0x27)
                lcdAda = true;
        }
        esp_task_wdt_reset();
    }
    if (!lcdAda)
        Serial.println("[HW] PERINGATAN: LCD 0x27 tidak terdeteksi (cek SDA/SCL, atau alamat 0x3F)");
}

void hwLcdBaris(uint8_t row, const char *txt)
{
    char b[21];
    snprintf(b, sizeof(b), "%-20s", txt);
    lcd.setCursor(0, row);
    lcd.print(b);
}

void hwTampilLcd()
{
    char b[24];
    hwLcdBaris(0, "TES HW - BACK 3s=out");
    snprintf(b, sizeof(b), "ENC %+ld CW%u CCW%u", hwEncPos, (unsigned)hwCw, (unsigned)hwCcw);
    hwLcdBaris(1, b);
    snprintf(b, sizeof(b), "OK:%u%c BACK:%u%c", (unsigned)hwSw.hitung, hwSw.stabil ? '*' : ' ',
             (unsigned)hwBack.hitung, hwBack.stabil ? '*' : ' ');
    hwLcdBaris(2, b);
    snprintf(b, sizeof(b), "bounce E%u O%u B%u", (unsigned)hwEncBounce, (unsigned)hwSw.bounce, (unsigned)hwBack.bounce);
    hwLcdBaris(3, b);
}

void hwTestMulai()
{
    if (hwTestAktif)
        return;
    if (!statusIdle())
    {
        LOGE("HW", "ditolak: HMI sedang bekerja (state=%d)", (int)currentState);
        return;
    }
    hwTestAktif = true;
    hwTestSelesaiMs = millis() + HW_TEST_DURASI_MS;
    hwEncPos = 0;
    hwCw = hwCcw = hwEncBounce = 0;
    hwEncDirty = false;
    hwInitTombol(hwSw, "OK", PIN_SW);
    hwInitTombol(hwBack, "BACK", PIN_BTN_PIN4);

    Serial.println("[HW] ===== MODE TES HARDWARE (tahan BACK 3 dtk / ketik 'hwtest off' untuk keluar) =====");
    Serial.printf("[HW] W5500 hardwareStatus=%d link=%d ip=%s\n", (int)Ethernet.hardwareStatus(),
                  (int)(Ethernet.linkStatus() != LinkOFF), Ethernet.localIP().toString().c_str());
    scanI2C();
    cekSemuaPin();
    hwLastClk = digitalRead(PIN_CLK);
    hwClkEdgeMs = millis();
    lcd.clear();
    hwTampilLcd();
    hwPublish("SYS", "hwtest_on", 0);
}

void hwTestSelesai(const char *alasan)
{
    hwTestAktif = false;
    Serial.printf("[HW] ===== SELESAI (%s) =====\n", alasan);
    Serial.printf("[HW] Ringkasan: ENC CW=%u CCW=%u bounce=%u | OK tekan=%u bounce=%u | BACK tekan=%u bounce=%u\n",
                  (unsigned)hwCw, (unsigned)hwCcw, (unsigned)hwEncBounce,
                  (unsigned)hwSw.hitung, (unsigned)hwSw.bounce,
                  (unsigned)hwBack.hitung, (unsigned)hwBack.bounce);
    if (hwEncBounce > 0 || hwSw.bounce > 0 || hwBack.bounce > 0)
        Serial.println("[HW] Bounce terdeteksi: pertimbangkan debounce perangkat lunak atau kapasitor 100nF pada pin terkait.");
    hwPublish("SYS", "hwtest_off", 0);

    // pulihkan layar & pembacaan encoder aplikasi
    lastModal = -1;
    lastStateCLK = digitalRead(PIN_CLK);
    lastInteractionMs = millis();
    layarKosongTergambar = false;
    if (currentState == STATE_PILIH_DATA)
        updateDisplayPilihData();
}

void hwTestLoop()
{
    hwProsesEncoder();
    hwProsesTombol(hwSw);
    hwProsesTombol(hwBack);

    unsigned long now = millis();
    if (hwBack.stabil && now - hwBack.tekanMs >= HW_TAHAN_KELUAR_MS)
    {
        hwTestSelesai("BACK ditahan 3 dtk");
        return;
    }
    if ((long)(now - hwTestSelesaiMs) >= 0)
    {
        hwTestSelesai("batas waktu 2 menit");
        return;
    }

    static unsigned long lastUi = 0;
    if (now - lastUi >= 200)
    {
        lastUi = now;
        hwTampilLcd();
        if (hwEncDirty)
        {
            hwEncDirty = false;
            hwPublish("ENC", "pos", hwEncPos);
        }
    }
}

// =====================================================
//  PERINTAH (MQTT: dititipkan lewat flag agar aman dari dalam callback)
//  Kirim ke topik pabrik/hmi_sanding/perintah:
//    HWTEST_ON, HWTEST_OFF, STATUS, MQTT_RECONNECT, DEBUG_0..DEBUG_3
// =====================================================
enum CmdDiag : uint8_t
{
    CMD_NONE = 0,
    CMD_HW_ON,
    CMD_HW_OFF,
    CMD_STATUS,
    CMD_MQTT_RECONNECT
};
volatile uint8_t cmdDiagTertunda = CMD_NONE;

bool perintahDiagnostik(const String &m)
{
    if (m == "HWTEST_ON")
        cmdDiagTertunda = CMD_HW_ON;
    else if (m == "HWTEST_OFF")
        cmdDiagTertunda = CMD_HW_OFF;
    else if (m == "STATUS")
        cmdDiagTertunda = CMD_STATUS;
    else if (m == "MQTT_RECONNECT")
        cmdDiagTertunda = CMD_MQTT_RECONNECT;
    else if (m.startsWith("DEBUG_"))
        logLevel = (uint8_t)constrain(m.substring(6).toInt(), 0, 3);
    else
        return false;
    return true;
}

void tickDiagnostik()
{
    uint8_t c = cmdDiagTertunda;
    cmdDiagTertunda = CMD_NONE;
    switch (c)
    {
    case CMD_HW_ON:
        hwTestMulai();
        break;
    case CMD_HW_OFF:
        if (hwTestAktif)
            hwTestSelesai("perintah MQTT");
        break;
    case CMD_STATUS:
        diagSegera = true;
        diag2Segera = true;
        break;
    case CMD_MQTT_RECONNECT:
        paksaSambungUlangMqtt("perintah MQTT");
        break;
    }

    unsigned long now = millis();
    if (diag2Segera || now - lastDiag2 >= 60000UL)
    {
        lastDiag2 = now;
        diag2Segera = false;
        publishDiag2();
    }
}

// =====================================================
//  PERINTAH SERIAL
// =====================================================
void cetakStatus()
{
    IPAddress ip = Ethernet.localIP();
    unsigned long upMqtt = (mqttConnectedSince == 0 || !client.connected()) ? 0 : (millis() - mqttConnectedSince) / 1000UL;
    Serial.println("------------- STATUS HMI -------------");
    Serial.printf("FW       : %s | up %lu mnt | reset: %s | log=%u\n", FW_VERSION, millis() / 60000UL, namaResetReason(), (unsigned)logLevel);
    Serial.printf("State    : %d (idle=%d) | sensor=%s | hwtest=%d\n", (int)currentState, (int)statusIdle(), sensorMode.c_str(), (int)hwTestAktif);
    Serial.printf("Ethernet : w5500=%d link=%d lanOK=%d ip=%u.%u.%u.%u | putus LAN=%lu\n", (int)Ethernet.hardwareStatus(),
                  (int)linkUp, (int)lanOK, ip[0], ip[1], ip[2], ip[3], (unsigned long)lanLostCount);
    Serial.printf("MQTT     : connected=%d state=%d (%s) | tersambung %lu dtk | putus=%lu\n", (int)client.connected(),
                  client.state(), namaErrorMQTT(client.state()), upMqtt, (unsigned long)mqttLostCount);
    Serial.printf("           coba=%lu ok=%lu gagal=%lu (beruntun %u) | retry=%lu ms | publish gagal=%lu\n",
                  (unsigned long)mqttTryCount, (unsigned long)mqttTryOk, (unsigned long)mqttTryFail,
                  (unsigned)mqttFailBeruntun, mqttRetryDelay, (unsigned long)mqttPubFail);
    Serial.printf("HTTP     : kode=%d ok=%lu gagal=%lu streak=%u | server=%s (%s)\n", lastHttpCode, (unsigned long)httpOkCount,
                  (unsigned long)httpFailCount, (unsigned)httpFailStreak, serverHostRuntime.c_str(), serverTls ? "HTTPS" : "HTTP");
    Serial.printf("Waktu    : NTP epoch=%lu\n", ntpEpoch);
    Serial.printf("Memori   : free=%lu min=%lu blok_terbesar=%lu\n", (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
    Serial.printf("Loop     : lag maksimum %lu ms (sejak publish diag2 terakhir)\n", loopLagMax);
    Serial.println("--------------------------------------");
}

void perintahSerialDiagnostik(String line)
{
    line.trim();
    if (line.length() == 0)
        return;
    String l = line;
    l.toLowerCase();

    if (l == "help")
    {
        Serial.println("Perintah: help | status | debug <0-3> | pins | i2c | hwtest | hwtest off |");
        Serial.println("          reconnect | pubtest | reboot | setkey <KEY> | setotakey <KEY>");
    }
    else if (l == "status")
        cetakStatus();
    else if (l.startsWith("debug"))
    {
        if (l.length() > 5)
            logLevel = (uint8_t)constrain(l.substring(5).toInt(), 0, 3);
        Serial.printf("[CFG] level log = %u (0=mati 1=error 2=info 3=debug)\n", (unsigned)logLevel);
    }
    else if (l == "pins")
        cekSemuaPin();
    else if (l == "i2c")
        scanI2C();
    else if (l == "hwtest")
        hwTestMulai();
    else if (l == "hwtest off")
    {
        if (hwTestAktif)
            hwTestSelesai("perintah serial");
    }
    else if (l == "reconnect")
        paksaSambungUlangMqtt("perintah serial");
    else if (l == "pubtest")
    {
        bool ok = client.connected() && client.publish(TOPIC_HMI "test", "ping", false);
        Serial.printf("[MQTT] pubtest: %s\n", ok ? "OK" : "GAGAL");
    }
    else if (l == "reboot")
        restartDevice("perintah serial");
    else
        Serial.println("[CFG] perintah tidak dikenal, ketik: help");
}