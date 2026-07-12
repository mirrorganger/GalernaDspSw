#include "app.h"

#include "Galerna/App/GalernaApp.hpp"
#include "Galerna/Core/TestTone.hpp"
#include "Galerna/Drivers/Es8388Codec.hpp"
#include "Galerna/Platform/Stm32F405/Stm32Gpio.hpp"
#include "Galerna/Platform/Stm32F405/Stm32I2c.hpp"

#include "main.h"

#include <array>
#include <cstdio>
#include <span>

extern "C" I2C_HandleTypeDef hi2c2;
extern "C" I2S_HandleTypeDef hi2s2;

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
galerna::core::TestTone testTone{4'096};

bool audioOutputEnabled{};
std::array<std::int16_t, 256> audioBuffer{};

void runCodecBringup()
{
    galerna::platform::stm32f405::Stm32I2c codecI2c{hi2c2};
    galerna::drivers::Es8388Codec codec{codecI2c};

    std::printf(
        "ES8388 codec bring-up: probing I2C address 0x%02X\r\n",
        galerna::drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);
    if (!codec.isPresent())
    {
        std::printf(
            "ES8388 codec bring-up: no ACK at 0x%02X\r\n",
            galerna::drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);
        return;
    }

    std::printf(
        "ES8388 codec bring-up: ACK at 0x%02X\r\n",
        galerna::drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);

    if (!codec.configureForI2sDacPlayback())
    {
        std::printf("ES8388 codec bring-up: register configuration failed\r\n");
        return;
    }

    std::printf("ES8388 codec bring-up: register configuration OK\r\n");
    audioOutputEnabled = true;
    std::printf("I2S test tone: enabled, 1 kHz sine at 32 kHz sample rate\r\n");
}

void transmitAudioTestTone()
{
    static auto transmitCount = std::uint32_t{};
    testTone.fillStereo(std::span<std::int16_t>{audioBuffer});
    const auto result = HAL_I2S_Transmit(
        &hi2s2,
        reinterpret_cast<std::uint16_t*>(audioBuffer.data()),
        static_cast<std::uint16_t>(audioBuffer.size()),
        100U);

    if (result != HAL_OK)
    {
        std::printf("I2S test tone: HAL_I2S_Transmit failed, status=%d\r\n", static_cast<int>(result));
        HAL_Delay(500);
        return;
    }

    ++transmitCount;
    if ((transmitCount % 2'000U) == 0U)
    {
        app.tick();
        std::printf("I2S test tone: transmitted %lu buffers\r\n", static_cast<unsigned long>(transmitCount));
    }
}

} // namespace

extern "C" void App_Init(void)
{
    app.init();
    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));
    runCodecBringup();
}

extern "C" void App_Tick(void)
{
    if (audioOutputEnabled)
    {
        transmitAudioTestTone();
        return;
    }

    app.tick();
    HAL_Delay(500);
}
