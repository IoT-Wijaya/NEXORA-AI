#pragma once

// Slot firmware yang bisa dituju saat rollback. Adaptor perangkat
// (src/esp_boot_slots.h) membungkus API OTA ESP-IDF; tes memakai slot palsu.
class IBootSlots
{
public:
    virtual ~IBootSlots() {}

    // Ada slot OTA lain (tempat firmware lama berada)?
    virtual bool hasPreviousSlot() = 0;
    // Isi slot lama tampak seperti image aplikasi yang utuh?
    virtual bool previousSlotValid() = 0;
    // Mengalihkan boot ke slot lama. Perangkat belum restart di sini.
    virtual bool setBootToPrevious() = 0;
    virtual const char *lastError() const = 0;
};