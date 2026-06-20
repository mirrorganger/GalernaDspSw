#include "Galerna/Platform/Stm32F405/Stm32I2c.hpp"

namespace galerna::platform::stm32f405
{

Stm32I2c::Stm32I2c(I2C_HandleTypeDef& handle)
    : handle_{handle}
{
}

bool Stm32I2c::write(std::uint8_t address, std::span<const std::uint8_t> data)
{
    return HAL_I2C_Master_Transmit(
               &handle_,
               static_cast<std::uint16_t>(address << 1U),
               const_cast<std::uint8_t*>(data.data()),
               static_cast<std::uint16_t>(data.size()),
               HAL_MAX_DELAY) == HAL_OK;
}

} // namespace galerna::platform::stm32f405
