#pragma once

#include "galerna/core/Range.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/hal/AdcConcept.hpp"
#include "galerna/hal/GpioConcept.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <ranges>

namespace galerna::app
{

template <hal::Gpio TStatusLed, hal::Adc TAdc, hal::Gpio TMuxGpio, hal::Gpio TDigitalIn>
class PotBlinkApp
{
public:
    static constexpr std::size_t ledCount{3};
    static constexpr std::size_t buttonCount{2};
    static constexpr std::size_t switchCount{2};
    static constexpr std::uint16_t potMaxValue{4'095};
    static constexpr core::Range blinkFrequencyHzRange{0.5F, 8.0F};

    PotBlinkApp(
        std::array<std::reference_wrapper<TStatusLed>, ledCount> statusLeds,
        drivers::PotMux4051<TAdc, TMuxGpio>& potMux,
        std::array<std::uint8_t, ledCount> potMuxChannels,
        std::array<std::reference_wrapper<TDigitalIn>, buttonCount> buttons,
        std::array<std::reference_wrapper<TDigitalIn>, switchCount> switches,
        std::uint32_t tickIntervalMs)
        : _statusLeds{statusLeds}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{buttons}
        , _switches{switches}
        , _tickIntervalMs{tickIntervalMs}
    {
    }

    void init()
    {
        for (auto& statusLed : _statusLeds)
        {
            statusLed.get().set(false);
        }
    }

    // Each LED blinks at a frequency proportional to its potentiometer's reading, unless
    // paused (either button held) or disabled (its switch off, LED0/LED1 only). Must be
    // called every _tickIntervalMs for the blink frequencies to be accurate.
    void tick()
    {
        // Buttons are active-low (pressed pulls the pin to GND through a pull-up).
        const bool paused = std::ranges::any_of(_buttons, [](auto& button) { return !button.get().get(); });

        std::array<bool, ledCount> ledEnabled{true, true, true};
        for (auto [enabled, toggleSwitch] : std::views::zip(ledEnabled, _switches))
        {
            enabled = toggleSwitch.get().get();
        }

        for (auto [led, channel, elapsedMs, ledState, enabled] :
             std::views::zip(_statusLeds, _potMuxChannels, _elapsedMs, _ledState, ledEnabled))
        {
            if (!enabled)
            {
                elapsedMs = 0U;
                ledState = false;
                led.get().set(false);
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
                led.get().set(ledState);
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

    std::array<std::reference_wrapper<TStatusLed>, ledCount> _statusLeds;
    drivers::PotMux4051<TAdc, TMuxGpio>& _potMux;
    std::array<std::uint8_t, ledCount> _potMuxChannels;
    std::array<std::reference_wrapper<TDigitalIn>, buttonCount> _buttons;
    std::array<std::reference_wrapper<TDigitalIn>, switchCount> _switches;
    std::uint32_t _tickIntervalMs;
    std::array<std::uint32_t, ledCount> _elapsedMs{};
    std::array<bool, ledCount> _ledState{};
};

} // namespace galerna::app
