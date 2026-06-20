#pragma once

#include <cstdint>
#include <span>

#include "stm32f4xx_hal.h"

namespace galerna::platform::stm32f405
{

class Stm32I2c
{
public:
    explicit Stm32I2c(I2C_HandleTypeDef& handle);

    bool write(std::uint8_t address, std::span<const std::uint8_t> data);

private:
    I2C_HandleTypeDef& handle_;
};

} // namespace galerna::platform::stm32f405
