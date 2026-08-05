#include "app.h"

#include "galerna/app/GalernaApp.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/platform/stm32f405/Stm32Adc.hpp"
#include "galerna/platform/stm32f405/Stm32Gpio.hpp"

#include "main.h"

#include <array>
#include <cstdio>

extern "C" ADC_HandleTypeDef hadc1;

namespace
{

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

galerna::app::GalernaApp app{statusLeds, potMux, ledPotMuxChannels, buttons, switches, appTickIntervalMs};

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
