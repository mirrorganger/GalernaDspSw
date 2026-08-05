#pragma once

#include <cstdint>

#include "stm32f4xx_hal.h"

namespace galerna::platform::stm32f405
{

class Stm32Adc
{
public:
    explicit Stm32Adc(ADC_HandleTypeDef& handle);

    std::uint16_t read(std::uint8_t channel);

private:
    ADC_HandleTypeDef& handle_;
};

} // namespace galerna::platform::stm32f405
