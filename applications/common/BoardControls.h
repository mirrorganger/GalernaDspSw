#pragma once

#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/platform/stm32f405/Stm32Adc.hpp"
#include "galerna/platform/stm32f405/Stm32Gpio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace galerna::app
{

inline constexpr std::size_t ledCount{3};
inline constexpr std::size_t buttonCount{2};
inline constexpr std::size_t switchCount{2};
inline constexpr std::size_t potMuxSelectLineCount{3};

// Full-scale reading behind every PotMux4051-read potentiometer (the board's ADC is 12-bit).
inline constexpr std::uint16_t potMaxValue{4'095};

inline constexpr std::size_t potMuxChannelCount{8U};

inline constexpr std::array<const char*, potMuxChannelCount> potMuxChannelName{
    "POT_3", "POT_5", "POT_1", "POT_7", "POT_2", "POT_4", "POT_6", "POT_8"};

struct BoardControls
{
    std::array<platform::stm32f405::Stm32Gpio, ledCount> statusLeds;
    std::array<platform::stm32f405::Stm32Gpio, buttonCount> buttons;
    std::array<platform::stm32f405::Stm32Gpio, switchCount> switches;
    platform::stm32f405::Stm32Adc potMuxAdc;
    std::array<platform::stm32f405::Stm32Gpio, potMuxSelectLineCount> potMuxSelectLines;
};

BoardControls makeBoardControls();

drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio> makePotMux(
    BoardControls& boardControls);

} // namespace galerna::app
