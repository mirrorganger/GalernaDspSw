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

// Port/pin identifying a not-yet-constructed Stm32Gpio -- lets PotBlinkApp build its own
// Stm32Gpio objects from a plain description instead of the caller constructing them upfront.
struct GpioPin
{
    GPIO_TypeDef* port;
    std::uint16_t pin;
};

namespace detail
{

// Index-sequence expansion so std::array<Stm32Gpio, N> can be built element-by-element from
// pins -- Stm32Gpio has no default constructor, so std::array<Stm32Gpio, N>{} isn't an option.
// Same trick as PotMux4051's detail::makeChannelBitsTable().
template <std::size_t N, std::size_t... I>
std::array<platform::stm32f405::Stm32Gpio, N> makeGpios(
    const std::array<GpioPin, N>& pins, std::index_sequence<I...>)
{
    return {platform::stm32f405::Stm32Gpio{pins[I].port, pins[I].pin}...};
}

template <std::size_t N>
std::array<platform::stm32f405::Stm32Gpio, N> makeGpios(const std::array<GpioPin, N>& pins)
{
    return makeGpios(pins, std::make_index_sequence<N>{});
}

} // namespace detail

// Only ever wired up against the real STM32 GPIO/ADC/mux, never against a host fake -- no
// template parameters needed. Owns its status LEDs/buttons/switches outright (plain
// Stm32Gpio, not reference_wrapper) since nothing outside this class needs to touch them --
// unlike _potMux, which is shared with app.cpp's printPotValues() diagnostic and so stays a
// reference to a longer-lived object. Builds those Stm32Gpio objects itself from GpioPin
// descriptions rather than having the caller construct them upfront.
class PotBlinkApp
{
public:
    static constexpr core::Range blinkFrequencyHzRange{0.5F, 8.0F};

    PotBlinkApp(
        std::array<GpioPin, ledCount> statusLedPins,
        drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& potMux,
        std::array<std::uint8_t, ledCount> potMuxChannels,
        std::array<GpioPin, buttonCount> buttonPins,
        std::array<GpioPin, switchCount> switchPins,
        std::uint32_t tickIntervalMs)
        : _statusLeds{detail::makeGpios(statusLedPins)}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{detail::makeGpios(buttonPins)}
        , _switches{detail::makeGpios(switchPins)}
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
