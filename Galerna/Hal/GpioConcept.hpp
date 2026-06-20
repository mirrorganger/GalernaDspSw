#pragma once

#include <concepts>

namespace galerna::hal
{

template <typename T>
concept Gpio = requires(T gpio, bool value)
{
    { gpio.set(value) } -> std::same_as<void>;
    { gpio.get() } -> std::convertible_to<bool>;
};

} // namespace galerna::hal
