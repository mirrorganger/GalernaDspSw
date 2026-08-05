#pragma once

#include <concepts>
#include <cstdint>

namespace galerna::hal
{

template <typename T>
concept Adc = requires(T adc, std::uint8_t channel)
{
    { adc.read(channel) } -> std::convertible_to<std::uint16_t>;
};

} // namespace galerna::hal
