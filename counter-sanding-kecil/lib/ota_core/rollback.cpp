#include "rollback.h"

const char *rollbackResultCode(RollbackResult result)
{
    switch (result)
    {
    case RollbackResult::Switched:
        return "switched";
    case RollbackResult::NoPreviousSlot:
        return "no_previous_slot";
    case RollbackResult::PreviousSlotInvalid:
        return "previous_slot_invalid";
    case RollbackResult::SwitchFailed:
        return "switch_failed";
    }
    return "unknown";
}

RollbackResult rollbackToPrevious(IBootSlots &slots)
{
    if (!slots.hasPreviousSlot())
        return RollbackResult::NoPreviousSlot;
    if (!slots.previousSlotValid())
        return RollbackResult::PreviousSlotInvalid;
    if (!slots.setBootToPrevious())
        return RollbackResult::SwitchFailed;
    return RollbackResult::Switched;
}