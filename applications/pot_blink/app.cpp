#include "app.h"

#include "BoardControls.h"
#include "galerna/core/Range.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/platform/stm32f405/Stm32Adc.hpp"
#include "galerna/platform/stm32f405/Stm32Gpio.hpp"

#include "main.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <ranges>

extern "C" ADC_HandleTypeDef hadc1;

namespace
{

// Only ever wired up against the real STM32 GPIO/ADC/mux, never against a host fake --
// no template parameters needed.
class PotBlinkApp
{
public:
    static constexpr galerna::core::Range blinkFrequencyHzRange{0.5F, 8.0F};

    PotBlinkApp(
        std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, galerna::app::ledCount>
            statusLeds,
        galerna::drivers::PotMux4051<galerna::platform::stm32f405::Stm32Adc, galerna::platform::stm32f405::Stm32Gpio>&
            potMux,
        std::array<std::uint8_t, galerna::app::ledCount> potMuxChannels,
        std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, galerna::app::buttonCount>
            buttons,
        std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, galerna::app::switchCount>
            switches,
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

        std::array<bool, galerna::app::ledCount> ledEnabled{true, true, true};
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
        const auto frequencyHz = blinkFrequencyHzRange.linear(
            static_cast<float>(potValue) / static_cast<float>(galerna::app::potMaxValue));
        return static_cast<std::uint32_t>(500.0F / frequencyHz);
    }

    std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, galerna::app::ledCount> _statusLeds;
    galerna::drivers::PotMux4051<galerna::platform::stm32f405::Stm32Adc, galerna::platform::stm32f405::Stm32Gpio>&
        _potMux;
    std::array<std::uint8_t, galerna::app::ledCount> _potMuxChannels;
    std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, galerna::app::buttonCount> _buttons;
    std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, galerna::app::switchCount> _switches;
    std::uint32_t _tickIntervalMs;
    std::array<std::uint32_t, galerna::app::ledCount> _elapsedMs{};
    std::array<bool, galerna::app::ledCount> _ledState{};
};

galerna::platform::stm32f405::Stm32Gpio statusLed0{LED_0_GPIO_Port, LED_0_Pin};
galerna::platform::stm32f405::Stm32Gpio statusLed1{LED_1_GPIO_Port, LED_1_Pin};
galerna::platform::stm32f405::Stm32Gpio statusLed2{LED_2_GPIO_Port, LED_2_Pin};

std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 3> statusLeds{
    std::ref(statusLed0),
    std::ref(statusLed1),
    std::ref(statusLed2)
};

galerna::platform::stm32f405::Stm32Adc potMuxAdc{hadc1};
galerna::platform::stm32f405::Stm32Gpio potMuxSel0{POT_MUX_SEL_0_GPIO_Port, POT_MUX_SEL_0_Pin};
galerna::platform::stm32f405::Stm32Gpio potMuxSel1{POT_MUX_SEL_1_GPIO_Port, POT_MUX_SEL_1_Pin};
galerna::platform::stm32f405::Stm32Gpio potMuxSel2{POT_MUX_SEL_2_GPIO_Port, POT_MUX_SEL_2_Pin};
std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 3> potMuxSelectLines{
    std::ref(potMuxSel0),
    std::ref(potMuxSel1),
    std::ref(potMuxSel2)
};
galerna::drivers::PotMux4051 potMux{potMuxAdc, potMuxSelectLines, std::uint8_t{0}};

constexpr std::uint8_t potMuxChannelCount{8U};

// Mux channel -> physical potentiometer reference, per GalernaDsp.kicad_sch
// (input_control sheet, U7 CD4051BM Y-pin nets), verified with `kicad-cli sch export netlist`.
constexpr std::array<const char*, potMuxChannelCount> potMuxChannelName{
    "POT_3", "POT_5", "POT_1", "POT_7", "POT_2", "POT_4", "POT_6", "POT_8"};

// LED0/1/2 blink at a rate driven by POT_1/2/3 respectively.
constexpr std::array<std::uint8_t, 3> ledPotMuxChannels{2U, 4U, 0U};

// BTN1/BTN2/SW1/SW2 -> MCU pin, per GalernaDsp.kicad_sch (input_control sheet),
// verified with `kicad-cli sch export netlist`. The silkscreen refs don't match the
// net numbers/names (e.g. BTN1 -> net PUSH_BTN_0, SW1 -> net SW_2), so this mapping
// is spelled out here rather than assumed from the Core/Inc/main.h pin names.
galerna::platform::stm32f405::Stm32Gpio btn1{PUSH_BTN_0_GPIO_Port, PUSH_BTN_0_Pin};
galerna::platform::stm32f405::Stm32Gpio btn2{PUSH_BTN_1_GPIO_Port, PUSH_BTN_1_Pin};
galerna::platform::stm32f405::Stm32Gpio sw1{SW_2_GPIO_Port, SW_2_Pin};
galerna::platform::stm32f405::Stm32Gpio sw2{SW_3_GPIO_Port, SW_3_Pin};
std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 2> buttons{
    std::ref(btn1), std::ref(btn2)};
std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 2> switches{
    std::ref(sw1), std::ref(sw2)};

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t potPrintIntervalTicks{50U}; // ~1 s at appTickIntervalMs

PotBlinkApp app{statusLeds, potMux, ledPotMuxChannels, buttons, switches, appTickIntervalMs};

void printPotValues()
{
    std::printf("Pots:");
    for (std::uint8_t channel = 0U; channel < potMuxChannelCount; ++channel)
    {
        std::printf(" %s=%u", potMuxChannelName[channel], static_cast<unsigned>(potMux.read(channel)));
    }
    std::printf("\r\n");
}

} // namespace

extern "C" void App_Init(void)
{
    app.init();
    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));
    std::printf("Pot-driven LED blink demo started\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    app.tick();

    ++tickCount;
    if ((tickCount % potPrintIntervalTicks) == 0U)
    {
        printPotValues();
    }

    HAL_Delay(appTickIntervalMs);
}
