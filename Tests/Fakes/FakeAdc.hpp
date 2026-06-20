#pragma once

#include <cstdint>

struct FakeAdc
{
    std::uint16_t read(std::uint8_t channel)
    {
        lastChannel = channel;
        return value;
    }

    std::uint8_t lastChannel{};
    std::uint16_t value{1234};
};
