#pragma once

struct FakeGpio
{
    void set(bool value)
    {
        state = value;
    }

    bool get() const
    {
        return state;
    }

    bool state{false};
};
