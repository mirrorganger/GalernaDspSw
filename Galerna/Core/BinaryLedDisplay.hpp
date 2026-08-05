#pragma once

#include "Galerna/Hal/GpioConcept.hpp"

#include <array>
#include <cstddef>
#include <functional>

namespace galerna::core
{

// Displays `value` in binary across `leds` (leds[0] is the least significant bit). Values that
// don't fit in LedCount bits are truncated -- only the low LedCount bits are shown.
template <hal::Gpio TGpio, std::size_t LedCount>
void displayBinary(std::array<std::reference_wrapper<TGpio>, LedCount>& leds, unsigned value)
{
    for (std::size_t bit = 0U; bit < LedCount; ++bit)
    {
        leds[bit].get().set(((value >> bit) & 1U) != 0U);
    }
}

} // namespace galerna::core
