#include "Galerna/Drivers/Es8388Codec.hpp"
#include "Tests/Fakes/FakeI2c.hpp"

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

TEST_CASE("ES8388 playback startup sequence writes documented DAC and output path registers")
{
    FakeI2c i2c;
    galerna::drivers::Es8388Codec codec{i2c};

    REQUIRE(codec.configureForI2sDacPlayback());

    const std::vector<I2cWrite> expectedWrites{
        {0x10, {0x00, 0x80}}, // Reset control port registers.
        {0x10, {0x00, 0x36}}, // Enable reference, VMID, same Fs, DAC MCLK source.
        {0x10, {0x01, 0x00}}, // Power up analog, bias generator and VREF buffer.
        {0x10, {0x02, 0x00}}, // Power up digital blocks, DLLs and DAC reference.
        {0x10, {0x03, 0xFC}}, // Keep ADC/input path powered down for playback-only smoke test.
        {0x10, {0x04, 0x3C}}, // Power up DAC L/R and enable LOUT/ROUT 1/2 drivers.
        {0x10, {0x05, 0x00}}, // Normal-power DAC/output operation.
        {0x10, {0x08, 0x00}}, // Slave serial-port mode; STM32 supplies clocks.
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
    };
    REQUIRE(i2c.writes == expectedWrites);
}

TEST_CASE("ES8388 startup sequence stops on first failed register write")
{
    FakeI2c i2c;
    i2c.failWriteAt = 2;
    galerna::drivers::Es8388Codec codec{i2c};

    REQUIRE_FALSE(codec.configureForI2sDacPlayback());

    REQUIRE(i2c.writes.size() == 2);
}
