#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct I2cWrite
{
    std::uint8_t address{};
    std::vector<std::uint8_t> data;

    bool operator==(const I2cWrite&) const = default;
};

struct FakeI2c
{
    bool isDeviceReady(std::uint8_t address)
    {
        probes.push_back(address);
        return nextProbeResult;
    }

    bool write(std::uint8_t address, std::span<const std::uint8_t> data)
    {
        writes.push_back({address, std::vector<std::uint8_t>{data.begin(), data.end()}});
        if (failWriteAt.has_value() && writes.size() == *failWriteAt)
        {
            return false;
        }
        return nextResult;
    }

    bool nextProbeResult{true};
    bool nextResult{true};
    std::optional<std::size_t> failWriteAt;
    std::vector<std::uint8_t> probes;
    std::vector<I2cWrite> writes;
};
