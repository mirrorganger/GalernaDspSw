#include "app.h"

#include "Galerna/App/GalernaApp.hpp"
#include "Galerna/Platform/Stm32F405/Stm32Gpio.hpp"

#include "main.h"

namespace
{

galerna::platform::stm32f405::Stm32Gpio statusLed{LED_0_GPIO_Port, LED_0_Pin};
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
