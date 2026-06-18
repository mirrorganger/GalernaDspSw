#include "app.h"

#include "main.h"

namespace {

class StatusLed {
public:
    void tick()
    {
        HAL_GPIO_TogglePin(LED_0_GPIO_Port, LED_0_Pin);
        HAL_Delay(500);
    }
};

StatusLed status_led;

} // namespace

extern "C" void App_Init(void)
{
}

extern "C" void App_Tick(void)
{
    status_led.tick();
}
