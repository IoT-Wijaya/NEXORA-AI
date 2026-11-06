#pragma once
#include <stdint.h>
#include "ilock.h"

struct CounterSnapshot
{
    uint32_t hasil;
    uint32_t modal;
};

// Penyimpan counter yang aman dipakai dua task.
// Semua data, termasuk bendera dirty, hanya disentuh di dalam kunci.
class CounterStore
{
public:
    explicit CounterStore(ILock &lock);

    void incrementHasil();
    void incrementModal();

    // Menolkan counter yang dipilih. SELALU menandai dirty walau angkanya
    // sudah nol, karena balasan ke HMI (START_OK, STOP_OK) bergantung padanya.
    void reset(bool resetHasil, bool resetModal);

    // Membaca kedua counter sebagai satu pasangan yang konsisten.
    CounterSnapshot snapshot() const;

    // true jika ada perubahan sejak panggilan terakhir, lalu bendera dihapus.
    // Urutan yang benar: takeDirty() DULU, snapshot() sesudahnya, sehingga
    // hitungan yang masuk di antaranya tidak terlewat.
    bool takeDirty();

private:
    ILock &lock_;
    uint32_t hasil_;
    uint32_t modal_;
    bool dirty_;
};