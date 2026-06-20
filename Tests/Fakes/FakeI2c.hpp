#pragma once

#include <cstdint>
#include <span>
#include <vector>

struct I2cWrite
{
    std::uint8_t address{};
    std::vector<std::uint8_t> data;
};

struct FakeI2c
{
    bool write(std::uint8_t address, std::span<const std::uint8_t> data)
    {
        writes.push_back({address, std::vector<std::uint8_t>{data.begin(), data.end()}});
        return nextResult;
    }

    bool nextResult{true};
    std::vector<I2cWrite> writes;
};
