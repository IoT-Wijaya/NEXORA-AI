#include "counter_store.h"

CounterStore::CounterStore(ILock &lock)
    : lock_(lock), hasil_(0), modal_(0), dirty_(false) {}

void CounterStore::incrementHasil()
{
    lock_.lock();
    hasil_++;
    dirty_ = true;
    lock_.unlock();
}

void CounterStore::incrementModal()
{
    lock_.lock();
    modal_++;
    dirty_ = true;
    lock_.unlock();
}

void CounterStore::reset(bool resetHasil, bool resetModal)
{
    lock_.lock();
    if (resetHasil)
        hasil_ = 0;
    if (resetModal)
        modal_ = 0;
    dirty_ = true;
    lock_.unlock();
}

CounterSnapshot CounterStore::snapshot() const
{
    lock_.lock();
    const CounterSnapshot s{hasil_, modal_};
    lock_.unlock();
    return s;
}

bool CounterStore::takeDirty()
{
    lock_.lock();
    const bool d = dirty_;
    dirty_ = false;
    lock_.unlock();
    return d;
}