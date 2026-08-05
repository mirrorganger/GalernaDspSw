#include "galerna/drivers/Es8388Codec.hpp"
#include "fakes/FakeI2c.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ES8388 detects the expected control address")
{
    FakeI2c i2c;
    galerna::drivers::Es8388Codec codec{i2c};

    REQUIRE(codec.isPresent());

    REQUIRE(i2c.probes.size() == 1);
    REQUIRE(i2c.probes[0] == 0x10);
}

TEST_CASE("ES8388 reset writes control register 0")
{
    FakeI2c i2c;
    galerna::drivers::Es8388Codec codec{i2c};

    REQUIRE(codec.reset());

    REQUIRE(i2c.writes.size() == 1);
    REQUIRE(i2c.writes[0].address == 0x10);
    REQUIRE(i2c.writes[0].data == std::vector<std::uint8_t>{0x00, 0x80});
}

TEST_CASE("ES8388 duplex startup sequence writes documented ADC, DAC and output path registers")
{
    FakeI2c i2c;
    galerna::drivers::Es8388Codec codec{i2c};

    REQUIRE(codec.configureForI2sDuplex());

    const std::vector<I2cWrite> expectedWrites{
        {0x10, {0x02, 0xF3}}, // Hold DEM/state-machine in reset while the rest is configured.
        {0x10, {0x2B, 0x80}}, // Force ADC and DAC to share this board's single LRCK line.
        {0x10, {0x00, 0x36}}, // Enable reference, VMID, same Fs, DAC MCLK source.
        {0x10, {0x01, 0x00}}, // Power up analog, bias generator and VREF buffer.
        {0x10, {0x03, 0x08}}, // Power up ADC L/R, analog input L/R and ADC bias gen.
        {0x10, {0x04, 0x3C}}, // Power up DAC L/R and enable LOUT/ROUT 1/2 drivers.
        {0x10, {0x0A, 0x00}}, // ADC input select: LIN1/RIN1.
        {0x10, {0x0B, 0x02}}, // Non-differential, stereo, ASDOUT not tri-stated (chip's own POR default).
        {0x10, {0x09, 0x00}}, // ADC PGA gain 0 dB.
        {0x10, {0x0C, 0x0C}}, // ADC: 16-bit Philips I2S.
        {0x10, {0x0D, 0x03}}, // ADC MCLK/Fs ratio 384.
        {0x10, {0x10, 0x00}}, // Left ADC digital volume 0 dB.
        {0x10, {0x11, 0x00}}, // Right ADC digital volume 0 dB.
        {0x10, {0x0F, 0x30}}, // ADC unmuted.
        {0x10, {0x17, 0x18}}, // DAC: 16-bit Philips I2S.
        {0x10, {0x18, 0x03}}, // DAC MCLK/Fs ratio 384 for 12.288 MHz / 32 kHz.
        {0x10, {0x19, 0x00}}, // DAC unmuted, no digital soft-ramp dependency.
        {0x10, {0x1A, 0x00}}, // Left DAC digital volume 0 dB.
        {0x10, {0x1B, 0x00}}, // Right DAC digital volume 0 dB.
        {0x10, {0x26, 0x00}}, // Output mixer input select at default analog inputs.
        {0x10, {0x27, 0x80}}, // Route left DAC to left output mixer only.
        {0x10, {0x2A, 0x80}}, // Route right DAC to right output mixer only.
        {0x10, {0x2E, 0x1E}}, // LOUT1 analog volume 0 dB.
        {0x10, {0x2F, 0x1E}}, // ROUT1 analog volume 0 dB.
        {0x10, {0x30, 0x1E}}, // LOUT2 analog volume 0 dB.
        {0x10, {0x31, 0x1E}}, // ROUT2 analog volume 0 dB.
        {0x10, {0x02, 0x00}}, // Release DEM/state-machine reset.
        {0x10, {0x08, 0x00}}, // Slave serial-port mode; must be written after the reset release.
    };
    REQUIRE(i2c.writes == expectedWrites);
}

TEST_CASE("ES8388 startup sequence stops on first failed register write")
{
    FakeI2c i2c;
    i2c.failWriteAt = 2;
    galerna::drivers::Es8388Codec codec{i2c};

    REQUIRE_FALSE(codec.configureForI2sDuplex());

    REQUIRE(i2c.writes.size() == 2);
}
