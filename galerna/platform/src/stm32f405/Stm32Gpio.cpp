#include "galerna/platform/stm32f405/Stm32Gpio.hpp"

namespace galerna::platform::stm32f405
{

Stm32Gpio::Stm32Gpio(GPIO_TypeDef* port, std::uint16_t pin)
    : port_{port}
    , pin_{pin}
{
}

void Stm32Gpio::set(bool value)
{
    HAL_GPIO_WritePin(port_, pin_, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool Stm32Gpio::get() const
{
    return HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET;
}

} // namespace galerna::platform::stm32f405
