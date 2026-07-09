#include "app.h"

#include "Galerna/App/GalernaApp.hpp"
#include "Galerna/Platform/Stm32F405/Stm32Gpio.hpp"

#include "main.h"

namespace
{

galerna::platform::stm32f405::Stm32Gpio statusLed0{LED_0_GPIO_Port, LED_0_Pin};
galerna::platform::stm32f405::Stm32Gpio statusLed1{LED_1_GPIO_Port, LED_1_Pin};
galerna::platform::stm32f405::Stm32Gpio statusLed2{LED_2_GPIO_Port, LED_2_Pin};

std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 3> statusLed{
    std::ref(statusLed0),
    std::ref(statusLed1),
    std::ref(statusLed2)
};


galerna::app::GalernaApp app{statusLed};

} // namespace

extern "C" void App_Init(void)
{
    app.init();
}

extern "C" void App_Tick(void)
{
    app.tick();
    HAL_Delay(500);
}
