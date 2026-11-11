#include "line_router.h"

bool LineRouter::addHandler(Handler handler)
{
    if (handlerCount_ >= MAX_HANDLERS || !handler)
        return false;
    handlers_[handlerCount_++] = handler;
    return true;
}

void LineRouter::forwardBuffered()
{
    if (!forward_)
        return;
    for (size_t i = 0; i < length_; ++i)
        forward_(line_[i]);
}

void LineRouter::feed(char c)
{
    const bool terminator = (c == '\r' || c == '\n');

    if (passthrough_)
    {
        if (forward_)
            forward_(c);
        if (terminator)
            passthrough_ = false;
        return;
    }

    if (terminator)
    {
        if (length_ == 0)
            return; // baris kosong, atau pasangan kedua dari "\r\n"
        line_[length_] = '\0';
        for (size_t i = 0; i < handlerCount_; ++i)
        {
            if (handlers_[i](line_))
            {
                length_ = 0;
                return;
            }
        }
        forwardBuffered();
        if (forward_)
            forward_('\n');
        length_ = 0;
        return;
    }

    if (length_ >= MAX_LINE)
    {
        forwardBuffered();
        length_ = 0;
        if (forward_)
            forward_(c);
        passthrough_ = true;
        return;
    }
    line_[length_++] = c;
}