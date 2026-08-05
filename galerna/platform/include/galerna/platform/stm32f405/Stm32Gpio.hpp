#pragma once

#include <cstdint>

#include "stm32f4xx_hal.h"

namespace galerna::platform::stm32f405
{

class Stm32Gpio
{
public:
    Stm32Gpio(GPIO_TypeDef* port, std::uint16_t pin);

    void set(bool value);
    bool get() const;

private:
    GPIO_TypeDef* port_{};
    std::uint16_t pin_{};
};

} // namespace galerna::platform::stm32f405
