#include "CodecBringup.h"

#include "galerna/drivers/Es8388Codec.hpp"
#include "galerna/platform/stm32f405/Stm32I2c.hpp"

#include "main.h"

#include <cstdio>

extern "C" I2C_HandleTypeDef hi2c2;

namespace galerna::app
{

bool runCodecBringup()
{
    platform::stm32f405::Stm32I2c codecI2c{hi2c2};
    drivers::Es8388Codec codec{codecI2c};

    std::printf(
        "ES8388 codec bring-up: probing I2C address 0x%02X\r\n",
        drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);
    if (!codec.isPresent())
    {
        std::printf(
            "ES8388 codec bring-up: no ACK at 0x%02X\r\n",
            drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);
        return false;
    }

    std::printf(
        "ES8388 codec bring-up: ACK at 0x%02X\r\n", drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);

    codec.reset();
    // The codec's internal reset routine needs time to settle before it reliably accepts
    // further register writes.
    HAL_Delay(10U);

    if (!codec.configureForI2sDuplex())
    {
        std::printf("ES8388 codec bring-up: register configuration failed\r\n");
        return false;
    }

    std::printf("ES8388 codec bring-up: register configuration OK\r\n");
    return true;
}

} // namespace galerna::app
