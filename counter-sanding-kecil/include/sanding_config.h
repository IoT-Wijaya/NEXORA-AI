#pragma once

// Pemilih konfigurasi. Isinya hanya #include, tanpa nilai.
// Mesin dipilih lewat build_flags di platformio.ini.
// Urutan penting: berkas mesin dulu (mendefinisikan TOPIC_BASE), baru general_config.h.

#if defined(MESIN_SANDING_KECIL)
#include "sanding_kecil_config.h"
#elif defined(MESIN_SANDING_BESAR)
#include "sanding_besar_config.h"
#else
#error "Mesin belum dipilih: tambahkan -DMESIN_SANDING_KECIL atau -DMESIN_SANDING_BESAR di build_flags"
#endif

#include "general_config.h"