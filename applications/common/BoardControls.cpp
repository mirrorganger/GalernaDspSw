#include "BoardControls.h"

#include "main.h"

#include <functional>

extern "C" ADC_HandleTypeDef hadc1;

namespace galerna::app
{

BoardControls makeBoardControls()
{
    return BoardControls{
        .statusLeds{
            platform::stm32f405::Stm32Gpio{LED_0_GPIO_Port, LED_0_Pin},
            platform::stm32f405::Stm32Gpio{LED_1_GPIO_Port, LED_1_Pin},
            platform::stm32f405::Stm32Gpio{LED_2_GPIO_Port, LED_2_Pin}},
        .buttons{
            platform::stm32f405::Stm32Gpio{PUSH_BTN_0_GPIO_Port, PUSH_BTN_0_Pin},
            platform::stm32f405::Stm32Gpio{PUSH_BTN_1_GPIO_Port, PUSH_BTN_1_Pin}},
        .switches{
            platform::stm32f405::Stm32Gpio{SW_2_GPIO_Port, SW_2_Pin},
            platform::stm32f405::Stm32Gpio{SW_3_GPIO_Port, SW_3_Pin}},
        .potMuxAdc{hadc1},
        .potMuxSelectLines{
            platform::stm32f405::Stm32Gpio{POT_MUX_SEL_0_GPIO_Port, POT_MUX_SEL_0_Pin},
            platform::stm32f405::Stm32Gpio{POT_MUX_SEL_1_GPIO_Port, POT_MUX_SEL_1_Pin},
            platform::stm32f405::Stm32Gpio{POT_MUX_SEL_2_GPIO_Port, POT_MUX_SEL_2_Pin}}};
}

drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio> makePotMux(
    BoardControls& boardControls)
{
    return drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>{
        boardControls.potMuxAdc,
        {std::ref(boardControls.potMuxSelectLines[0]),
         std::ref(boardControls.potMuxSelectLines[1]),
         std::ref(boardControls.potMuxSelectLines[2])},
        std::uint8_t{0}};
}

} // namespace galerna::app
