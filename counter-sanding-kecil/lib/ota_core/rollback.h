#pragma once
#include "iboot_slots.h"

enum class RollbackResult
{
    Switched,            // slot boot sudah dialihkan ke firmware lama
    NoPreviousSlot,      // tidak ada slot lain
    PreviousSlotInvalid, // slot lama kosong atau rusak, tidak dicoba dialihkan
    SwitchFailed,        // pengalihan ditolak
};

const char *rollbackResultCode(RollbackResult result);

// Mengalihkan boot ke firmware lama dengan urutan yang aman:
// cek slot ada -> cek isinya -> baru alihkan. Slot boot TIDAK disentuh
// kecuali kedua pemeriksaan lolos. Memanggil restart adalah urusan pemanggil.
RollbackResult rollbackToPrevious(IBootSlots &slots);