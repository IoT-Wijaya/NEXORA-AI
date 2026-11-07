#pragma once
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// Mencetak slot firmware yang berjalan dan tujuan OTA berikutnya.
// Jika salah satunya tidak ada, tabel partisi tidak mendukung OTA.
inline void logPartition(const char *judul, const esp_partition_t *p)
{
    if (p == nullptr)
    {
        Serial.printf("%s: TIDAK ADA (OTA tidak didukung)\n", judul);
        return;
    }
    Serial.printf("%s: %s @0x%06X, ukuran 0x%06X\n", judul, p->label,
                  (unsigned)p->address, (unsigned)p->size);
}

inline void logPartitions()
{
    logPartition("Slot berjalan  ", esp_ota_get_running_partition());
    logPartition("Slot OTA berikut", esp_ota_get_next_update_partition(nullptr));
    logPartition("Data OTA       ",
                 esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr));
}