#pragma once

#include "Galerna/Hal/I2cConcept.hpp"

#include <array>
#include <cstdint>

namespace galerna::drivers
{

template <hal::I2cBus TI2c>
class Wm8731Codec
{
public:
    explicit Wm8731Codec(TI2c& i2c)
        : i2c_{i2c}
    {
    }

    bool reset()
    {
        return writeRegister(0x0F, 0x000);
    }

    bool writeRegister(std::uint8_t reg, std::uint16_t value)
    {
        const std::array<std::uint8_t, 2> data{
            static_cast<std::uint8_t>((reg << 1U) | ((value >> 8U) & 0x01U)),
            static_cast<std::uint8_t>(value & 0xFFU)
        };

        return i2c_.write(deviceAddress, data);
    }

    static constexpr std::uint8_t deviceAddress{0x1A};

private:
    TI2c& i2c_;
};

} // namespace galerna::drivers
