#pragma once

#include "Galerna/Hal/I2cConcept.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace galerna::drivers{

template <hal::I2cBus TI2c>
class Es8388Codec
{
public:
    explicit Es8388Codec(TI2c& i2c)
        : _i2c{i2c}
    {
    }

    bool isPresent()
    {
        return _i2c.isDeviceReady(deviceAddress);
    }

    bool reset()
    {
        return writeRegister({0x00, 0x80});
    }

    bool configureForI2sDacPlayback()
    {
        // Register values follow the ES8388 datasheet playback path:
        // - STM32 is I2S master, so ES8388 serial port is slave (Reg. 08 MSC=0).
        // - CubeMX currently generates 32 kHz I2S with MCLK enabled, so DAC FsRatio is 384.
        // - Route DAC L/R into the output mixers and enable both output-driver pairs.
        constexpr std::array<RegisterWrite, 20> script{{
            {0x00, 0x80}, // Reset control port registers.
            {0x00, 0x36}, // Enable reference, VMID=500 kOhm, same Fs, DAC MCLK source.
            {0x01, 0x00}, // Power up analog, bias generator and VREF buffer.
            {0x02, 0x00}, // Power up digital blocks, DLLs and DAC reference.
            {0x03, 0xFC}, // Keep ADC/input path powered down for playback-only smoke test.
            {0x04, 0x3C}, // Power up DAC L/R and enable LOUT/ROUT 1/2 drivers.
            {0x05, 0x00}, // Normal-power DAC/output operation.
            {0x08, 0x00}, // Slave serial-port mode; STM32 supplies clocks.
            {0x17, 0x18}, // DAC: 16-bit Philips I2S.
            {0x18, 0x03}, // DAC MCLK/Fs ratio 384 for 12.288 MHz / 32 kHz.
            {0x19, 0x00}, // DAC unmuted, no digital soft-ramp dependency.
            {0x1A, 0x00}, // Left DAC digital volume 0 dB.
            {0x1B, 0x00}, // Right DAC digital volume 0 dB.
            {0x26, 0x00}, // Keep output mixer input-select defaults.
            {0x27, 0x80}, // Route left DAC to left output mixer only.
            {0x2A, 0x80}, // Route right DAC to right output mixer only.
            {0x2E, 0x1E}, // LOUT1 analog volume 0 dB.
            {0x2F, 0x1E}, // ROUT1 analog volume 0 dB.
            {0x30, 0x1E}, // LOUT2 analog volume 0 dB.
            {0x31, 0x1E}, // ROUT2 analog volume 0 dB.
        }};
        return writeRegisters(script);
    }

    static constexpr std::uint8_t deviceAddress{0x10};

private:
    struct RegisterWrite
    {
        std::uint8_t reg;
        std::uint8_t value;
    };

    bool writeRegisters(std::span<const RegisterWrite> registersWrite)
    {
        for (const auto& registerWrite : registersWrite)
        {
            if (!writeRegister(registerWrite))
            {
                return false;
            }
        }
        return true;
    }

    bool writeRegister(const RegisterWrite& registerWrite)
    {
        const std::array<std::uint8_t, 2> data{registerWrite.reg, registerWrite.value};
        return _i2c.write(deviceAddress, data);
    }


    TI2c& _i2c;
};

} // namespace galerna::drivers
