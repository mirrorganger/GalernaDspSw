#pragma once

#include <concepts>
#include <cstdint>
#include <span>

namespace galerna::hal
{

template <typename T>
concept I2cBus = requires(T bus, std::uint8_t address, std::span<const std::uint8_t> data)
{
    { bus.write(address, data) } -> std::same_as<bool>;
};

} // namespace galerna::hal
