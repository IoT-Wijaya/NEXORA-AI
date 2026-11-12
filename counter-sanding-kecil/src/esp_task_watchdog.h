#pragma once
#include <stddef.h>
#include <Arduino.h>
#include <esp_task_wdt.h>

// Pembungkus watchdog tugas ESP32. Task yang macet (tidak memanggil feed() dalam batas
// waktu) membuat perangkat reset sendiri. Setiap reset menambah hitungan boot Pending di
// OtaStateMachine, sehingga firmware baru yang selalu macet berakhir di rollback.
//
// API berbeda antara core Arduino 2.x (ESP-IDF 4.x) dan 3.x (ESP-IDF 5.x); dipilih lewat
// ESP_ARDUINO_VERSION_MAJOR.
class EspTaskWatchdog
{
public:
    // Panggil sekali di setup(), SEBELUM task dibuat. Mengatur batas waktu dan reset otomatis.
    static bool begin(uint32_t timeoutSeconds)
    {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        // Core 3.x sudah menyalakan TWDT; idle task kedua core tetap diawasi seperti bawaan core.
        esp_task_wdt_config_t cfg = {};
        cfg.timeout_ms = timeoutSeconds * 1000UL;
        cfg.idle_core_mask = (1U << portNUM_PROCESSORS) - 1U;
        cfg.trigger_panic = true;
        return esp_task_wdt_reconfigure(&cfg) == ESP_OK;
#else
        // Core 2.x: bila sudah menyala, pemanggilan ini hanya memperbarui batas waktu dan panic.
        return esp_task_wdt_init(timeoutSeconds, true) == ESP_OK;
#endif
    }

    // Dipanggil di awal fungsi task yang akan diawasi, dari dalam task itu sendiri.
    static bool subscribeCurrentTask() { return esp_task_wdt_add(NULL) == ESP_OK; }

    // Dipanggil sekali setiap putaran loop task.
    static void feed() { esp_task_wdt_reset(); }
};