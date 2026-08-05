#pragma once

#include "galerna/hal/I2cConcept.hpp"

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

    bool configureForI2sDuplex()
    {
        // Register values follow the ES8388 datasheet's "Sequence for Start up codec"
        // (section 10.1, simultaneous record + playback), adapted to this board:
        // - STM32 is I2S master, so ES8388 serial port is slave (Reg. 08 MSC=0).
        // - This board has a single shared WS/LRCK line (no separate ADC/DAC LRCK pins), so
        //   Reg. 0x2B (slrck=1) forces ADC and DAC to use the same LRCK, per the datasheet's
        //   explicit note that this is required whenever both ADC and DAC run together.
        // - CubeMX currently generates 32 kHz I2S with MCLK enabled, so both DAC and ADC
        //   FsRatio are 384.
        // - Line-in is wired to LIN1/RIN1 (verified via `kicad-cli sch export netlist` on
        //   audio_codec.kicad_sch; LIN2/RIN2 are unconnected), single-ended, no mic PGA boost.
        // - Reg. 0x0B (TRI) is written explicitly rather than left at its reset default: the
        //   datasheet's own bit table and its register-default header disagree on what that
        //   default actually is, and TRI=1 would tri-state ASDOUT (the ADC's serial data pin),
        //   which would silently produce exactly this: DMA capturing constant idle/floating
        //   samples with no NACK or error anywhere to reveal it.
        // - Route DAC L/R into the output mixers and enable both output-driver pairs.
        // - Does NOT include the reset write (call reset() first) -- the codec's internal reset
        //   routine needs time to settle before it reliably accepts register writes at all.
        // - Reg. 0x08 (MSC, master/slave select) is written LAST, after Reg. 0x02 releases the
        //   DEM/state-machine reset, not first as the datasheet's own suggested sequence has it:
        //   confirmed via I2C readback that a write to Reg. 0x08 while the DEM/state-machine is
        //   still held in reset is silently ACKed on the bus but never actually latched by the
        //   chip -- it stays at its power-on default (MSC=1, master mode, contending with the
        //   STM32 for the WS/CK lines) until written again after the reset release, which does
        //   stick.
        constexpr std::array<RegisterWrite, 28> script{{
            {0x02, 0xF3}, // Hold DEM/state-machine in reset while the rest is configured.
            {0x2B, 0x80}, // Force ADC and DAC to share this board's single LRCK line.
            {0x00, 0x36}, // Enable reference, VMID=500 kOhm, same Fs, DAC MCLK source.
            {0x01, 0x00}, // Power up analog, bias generator and VREF buffer.
            {0x03, 0x08}, // Power up ADC L/R, analog input L/R and ADC bias gen; mic bias stays down.
            {0x04, 0x3C}, // Power up DAC L/R and enable LOUT/ROUT 1/2 drivers.
            {0x0A, 0x00}, // ADC input select: LIN1/RIN1 (line-in jack).
            {0x0B, 0x02}, // Non-differential input, stereo (no mono-mix), ASDOUT NOT tri-stated.
                          // The chip's own documented power-on default for this register is
                          // 0x02, not 0x00: bit 1 is unnamed/reserved in the datasheet's own bit
                          // table, but every reference (the datasheet's own example writes, the
                          // Espressif ESP-ADF driver's es8388_init()) always writes 0x02 here,
                          // never 0x00. Clearing that undocumented bit is the one place this
                          // script deviated from every known-working reference.
            {0x09, 0x00}, // ADC PGA gain 0 dB (line level input, no mic boost).
            {0x0C, 0x0C}, // ADC: 16-bit Philips I2S.
            {0x0D, 0x03}, // ADC MCLK/Fs ratio 384, matching the DAC ratio (shared MCLK/Fs).
            {0x10, 0x00}, // Left ADC digital volume 0 dB (register default is -96 dB).
            {0x11, 0x00}, // Right ADC digital volume 0 dB (register default is -96 dB).
            {0x0F, 0x30}, // ADC unmuted (soft-ramp enabled, default ramp rate).
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
            {0x02, 0x00}, // Release DEM/state-machine reset now that ADC+DAC are configured.
            {0x08, 0x00}, // Slave serial-port mode; STM32 supplies clocks. Must be written here.
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
