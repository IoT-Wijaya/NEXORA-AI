#pragma once
#include <stdint.h>

enum class OtaCommandResult
{
    NotOta,      // bukan perintah "ota": serahkan ke penangan lain
    Ok,          // "ota <build>" sah
    BadArgument, // diawali "ota" tetapi argumennya salah
};

namespace OtaCommand
{

    // Membaca baris "ota <nomor build>", misalnya "ota 2". Nama perintah tidak
    // peka huruf besar/kecil; spasi di awal, tengah, dan akhir diabaikan.
    // Nomor build harus 1 sampai 4294967295, hanya angka, tanpa tambahan apa pun.
    // `build` hanya diisi bila hasilnya Ok.
    OtaCommandResult parse(const char *line, uint32_t &build);

} // namespace OtaCommand