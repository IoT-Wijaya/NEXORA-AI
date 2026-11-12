#pragma once
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "iboot_slots.h"

// Adaptor perangkat: IBootSlots di atas API OTA ESP-IDF.
// "Slot lama" = slot OTA yang BUKAN sedang berjalan. Saat firmware baru berjalan,
// itu adalah tempat firmware sebelumnya.
class EspBootSlots : public IBootSlots
{
public:
    bool hasPreviousSlot() override
    {
        previous_ = esp_ota_get_next_update_partition(nullptr);
        if (previous_ == nullptr)
            return fail("slot OTA lain tidak ada");
        return true;
    }

    bool previousSlotValid() override
    {
        if (previous_ == nullptr)
            return fail("slot lama belum dipilih");
        esp_app_desc_t desc;
        if (esp_ota_get_partition_description(previous_, &desc) != ESP_OK)
            return fail("slot lama tidak berisi image aplikasi");
        return true;
    }

    // esp_ota_set_boot_partition memverifikasi image penuh (checksum dan hash) sebelum
    // menulis otadata, jadi image rusak ditolak di sini.
    bool setBootToPrevious() override
    {
        if (previous_ == nullptr)
            return fail("slot lama belum dipilih");
        if (esp_ota_set_boot_partition(previous_) != ESP_OK)
            return fail("esp_ota_set_boot_partition menolak slot lama");
        return true;
    }

    const char *lastError() const override { return error_; }

private:
    bool fail(const char *text)
    {
        error_ = text;
        return false;
    }

    const esp_partition_t *previous_ = nullptr;
    const char *error_ = "";
};