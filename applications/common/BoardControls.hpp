#pragma once

#include <cstddef>
#include <cstdint>

namespace galerna::app
{

// Physical control counts on the Galerna DSP board (see docs/Design.md) -- shared by every
// app that wires up status LEDs/buttons/switches, not just the ones that use all three.
inline constexpr std::size_t ledCount{3};
inline constexpr std::size_t buttonCount{2};
inline constexpr std::size_t switchCount{2};

// Full-scale reading behind every PotMux4051-read potentiometer (the board's ADC is 12-bit).
inline constexpr std::uint16_t potMaxValue{4'095};

} // namespace galerna::app
