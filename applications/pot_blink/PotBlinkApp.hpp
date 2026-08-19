#pragma once

#include "BoardControls.h"
#include "galerna/core/Range.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/platform/stm32f405/Stm32Adc.hpp"
#include "galerna/platform/stm32f405/Stm32Gpio.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <utility>

namespace galerna::app
{

class PotBlinkApp
{
public:
    static constexpr core::Range blinkFrequencyHzRange{0.5F, 8.0F};

    PotBlinkApp(
        std::array<platform::stm32f405::Stm32Gpio, ledCount> statusLeds,
        drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& potMux,
        std::array<std::uint8_t, ledCount> potMuxChannels,
        std::array<platform::stm32f405::Stm32Gpio, buttonCount> buttons,
        std::array<platform::stm32f405::Stm32Gpio, switchCount> switches,
        std::uint32_t tickIntervalMs)
        : _statusLeds{std::move(statusLeds)}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{std::move(buttons)}
        , _switches{std::move(switches)}
        , _tickIntervalMs{tickIntervalMs}
    {
    }

    void init()
    {
        for (auto& statusLed : _statusLeds)
        {
            statusLed.set(false);
        }
    }

    // Each LED blinks at a frequency proportional to its potentiometer's reading, unless
    // paused (either button held) or disabled (its switch off, LED0/LED1 only). Must be
    // called every _tickIntervalMs for the blink frequencies to be accurate.
    void tick()
    {
        // Buttons are active-low (pressed pulls the pin to GND through a pull-up).
        const bool paused = std::ranges::any_of(_buttons, [](auto& button) { return !button.get(); });

        std::array<bool, ledCount> ledEnabled{true, true, true};
        for (auto [enabled, toggleSwitch] : std::views::zip(ledEnabled, _switches))
        {
            enabled = toggleSwitch.get();
        }

        for (auto [led, channel, elapsedMs, ledState, enabled] :
             std::views::zip(_statusLeds, _potMuxChannels, _elapsedMs, _ledState, ledEnabled))
        {
            if (!enabled)
            {
                elapsedMs = 0U;
                ledState = false;
                led.set(false);
                continue;
            }

            if (paused)
            {
                continue;
            }

            const auto potValue = _potMux.read(channel);

            elapsedMs += _tickIntervalMs;
            if (elapsedMs >= blinkHalfPeriodMs(potValue))
            {
                elapsedMs = 0U;
                ledState = !ledState;
                led.set(ledState);
            }
        }
    }

private:
    static std::uint32_t blinkHalfPeriodMs(std::uint16_t potValue)
    {
        const auto frequencyHz
            = blinkFrequencyHzRange.linear(static_cast<float>(potValue) / static_cast<float>(potMaxValue));
        return static_cast<std::uint32_t>(500.0F / frequencyHz);
    }

    std::array<platform::stm32f405::Stm32Gpio, ledCount> _statusLeds;
    drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& _potMux;
    std::array<std::uint8_t, ledCount> _potMuxChannels;
    std::array<platform::stm32f405::Stm32Gpio, buttonCount> _buttons;
    std::array<platform::stm32f405::Stm32Gpio, switchCount> _switches;
    std::uint32_t _tickIntervalMs;
    std::array<std::uint32_t, ledCount> _elapsedMs{};
    std::array<bool, ledCount> _ledState{};
};

} // namespace galerna::app
