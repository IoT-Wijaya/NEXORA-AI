#pragma once

// Bukti bahwa server (broker MQTT) benar-benar menerima pesan dari perangkat.
// Dipakai pengawas kesehatan OTA sebagai nilai serverOk.
//
// Cara pakai, satu putaran setiap MQTT berhasil tersambung:
//   begin()          -> mulai putaran baru, bukti lama dihapus
//   record(hasil)    -> dipanggil untuk SETIAP publish, hasil = true bila diterima klien MQTT
//   finish()         -> semua publish putaran ini sudah dicoba
// ok() bernilai true hanya bila putaran selesai dan tidak ada satu pun publish yang gagal.
class ServerProof
{
public:
    ServerProof() : started_(false), allAccepted_(true), finished_(false) {}

    void begin()
    {
        started_ = true;
        allAccepted_ = true;
        finished_ = false;
    }

    void record(bool accepted)
    {
        if (!started_ || finished_)
            return; // abaikan publish di luar putaran (mis. heartbeat)
        if (!accepted)
            allAccepted_ = false;
    }

    void finish()
    {
        if (started_)
            finished_ = true;
    }

    bool ok() const { return started_ && finished_ && allAccepted_; }

private:
    bool started_;
    bool allAccepted_;
    bool finished_;
};