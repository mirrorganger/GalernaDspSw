#include "app.h"

#include "Applications/ThxDeepNote/ThxDeepNoteApp.hpp"
#include "Galerna/Core/DuplexAudioBlockProcessor.hpp"
#include "Galerna/Core/ProcessorChain.hpp"
#include "Galerna/Drivers/Es8388Codec.hpp"
#include "Galerna/Drivers/PotMux4051.hpp"
#include "Galerna/Effects/ThxDeepNote.hpp"
#include "Galerna/Platform/Stm32F405/Stm32Adc.hpp"
#include "Galerna/Platform/Stm32F405/Stm32Gpio.hpp"
#include "Galerna/Platform/Stm32F405/Stm32I2c.hpp"
#include "Galerna/Platform/Stm32F405/Stm32I2sDuplexAudio.hpp"

#include "main.h"

#include <array>
#include <cstdio>

extern "C" ADC_HandleTypeDef hadc1;
extern "C" I2C_HandleTypeDef hi2c2;
extern "C" I2S_HandleTypeDef hi2s2;

namespace
{

galerna::platform::stm32f405::Stm32Gpio statusLed0{LED_0_GPIO_Port, LED_0_Pin};
galerna::platform::stm32f405::Stm32Gpio statusLed1{LED_1_GPIO_Port, LED_1_Pin};
galerna::platform::stm32f405::Stm32Gpio statusLed2{LED_2_GPIO_Port, LED_2_Pin};

std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 3> statusLeds{
    std::ref(statusLed0),
    std::ref(statusLed1),
    std::ref(statusLed2)
};

galerna::platform::stm32f405::Stm32Adc potMuxAdc{hadc1};
galerna::platform::stm32f405::Stm32Gpio potMuxSel0{POT_MUX_SEL_0_GPIO_Port, POT_MUX_SEL_0_Pin};
galerna::platform::stm32f405::Stm32Gpio potMuxSel1{POT_MUX_SEL_1_GPIO_Port, POT_MUX_SEL_1_Pin};
galerna::platform::stm32f405::Stm32Gpio potMuxSel2{POT_MUX_SEL_2_GPIO_Port, POT_MUX_SEL_2_Pin};
std::array<std::reference_wrapper<galerna::platform::stm32f405::Stm32Gpio>, 3> potMuxSelectLines{
    std::ref(potMuxSel0),
    std::ref(potMuxSel1),
    std::ref(potMuxSel2)
};
galerna::drivers::PotMux4051 potMux{potMuxAdc, potMuxSelectLines, std::uint8_t{0}};

constexpr std::uint8_t potMuxChannelCount{8U};

// Mux channel -> physical potentiometer reference, per GalernaDsp.kicad_sch
// (input_control sheet, U7 CD4051BM Y-pin nets), verified with `kicad-cli sch export netlist`.
constexpr std::array<const char*, potMuxChannelCount> potMuxChannelName{
    "POT_3", "POT_5", "POT_1", "POT_7", "POT_2", "POT_4", "POT_6", "POT_8"};

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t potPrintIntervalTicks{50U}; // ~1 s at appTickIntervalMs

// THX Deep Note synth -> line-out, driven continuously by DMA (see Stm32I2sDuplexAudio). Real
// line-in capture is currently dead at the hardware level (ADC/ASDOUT stuck high -- see
// docs/progress.md), so ThxDeepNote ignores whatever garbage the DMA rx half contains and
// generates its own signal; the rx half of the DMA transfer keeps running regardless (that's how
// full-duplex I2S works), it's just unused.
constexpr std::size_t audioFramesPerHalf{64U}; // 64 stereo frames = 2 ms per half at 32 kHz.
// The true I2S sample rate, computed from live register reads (RCC->PLLI2SCFGR: PLLI2SN=50,
// PLLI2SR=2; SPI2->I2SPR: I2SDIV=3, ODD=0, MCKOE=1): I2SCLK = (HSE/PLLM)*PLLI2SN/PLLI2SR =
// (24 MHz/12)*50/2 = 50 MHz; Fs = I2SCLK / (256*(2*I2SDIV+ODD)) = 50 MHz/1536 -- not exactly the
// nominal 32 kHz the ES8388 register script assumes (see docs/progress.md).
constexpr float audioSampleRateHz{50'000'000.0F / 1536.0F};
galerna::effects::ThxDeepNote audioThx;
galerna::core::ProcessorChain<galerna::effects::ThxDeepNote> audioChain{audioThx};
galerna::core::DuplexAudioBlockProcessor<audioFramesPerHalf, galerna::effects::ThxDeepNote> audioProcessor{
    audioChain};
galerna::platform::stm32f405::Stm32I2sDuplexAudio<
    audioFramesPerHalf,
    galerna::core::DuplexAudioBlockProcessor<audioFramesPerHalf, galerna::effects::ThxDeepNote>>
    audioEngine{hi2s2, audioProcessor};

// THX Deep Note synth controls, one PotMux4051 channel each (channels not already claimed by
// the (currently unused in this app) LED-blink demo -- see Applications/PotBlink/app.cpp).
galerna::app::ThxDeepNoteApp<
    galerna::platform::stm32f405::Stm32Gpio,
    galerna::platform::stm32f405::Stm32Adc,
    galerna::platform::stm32f405::Stm32Gpio,
    decltype(audioEngine),
    galerna::effects::ThxDeepNote>
    thxApp{
        statusLeds,
        potMux,
        {.pitch = 1U, .pitchShift = 3U, .timbre = 5U, .resonance = 7U, .voiceCount = 6U},
        audioEngine,
        audioThx,
        audioSampleRateHz};

void printPotValues()
{
    std::printf("Pots:");
    for (std::uint8_t channel = 0U; channel < potMuxChannelCount; ++channel)
    {
        std::printf(" %s=%u", potMuxChannelName[channel], static_cast<unsigned>(potMux.read(channel)));
    }
    std::printf("\r\n");

    // Budget: how many CPU cycles one half-buffer's worth of real time actually allows, vs. the
    // worst-case cycles the DSP work has taken (DWT cycle counter, see Stm32I2sDuplexAudio).
    // Printed as an integer-math percentage (tenths) to avoid relying on newlib-nano's optional
    // float printf support.
    const auto budgetCycles = static_cast<std::uint32_t>(
        static_cast<float>(SystemCoreClock) * static_cast<float>(audioFramesPerHalf) / audioSampleRateHz);
    const auto maxProcessCycles = audioEngine.maxProcessCycles();
    const std::uint32_t budgetPercentTenths = budgetCycles > 0U
        ? static_cast<std::uint32_t>((static_cast<std::uint64_t>(maxProcessCycles) * 1000U) / budgetCycles)
        : 0U;
    std::printf(
        "Audio DMA: callCount=%lu errorCount=%lu maxProcessCycles=%lu/%lu (%lu.%lu%%) clipCount=%lu\r\n",
        static_cast<unsigned long>(audioEngine.callCount()),
        static_cast<unsigned long>(audioEngine.errorCount()),
        static_cast<unsigned long>(maxProcessCycles),
        static_cast<unsigned long>(budgetCycles),
        static_cast<unsigned long>(budgetPercentTenths / 10U),
        static_cast<unsigned long>(budgetPercentTenths % 10U),
        static_cast<unsigned long>(audioProcessor.clipCount()));
}

// Codec bring-up needs a real HAL_Delay() between reset() and the register-configuration
// script, so it stays a free function here (a genuine HAL passthrough) rather than living in
// the host-testable ThxDeepNoteApp class.
bool runCodecBringup()
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
        return false;
    }

    std::printf(
        "ES8388 codec bring-up: ACK at 0x%02X\r\n",
        galerna::drivers::Es8388Codec<decltype(codecI2c)>::deviceAddress);

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

} // namespace

extern "C" void App_Init(void)
{
    for (auto& statusLed : statusLeds)
    {
        statusLed.get().set(false);
    }

    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));

    if (!runCodecBringup())
    {
        return;
    }

    if (!thxApp.init())
    {
        std::printf("I2S duplex audio: HAL_I2SEx_TransmitReceive_DMA failed to start\r\n");
        return;
    }

    std::printf("I2S duplex audio: started (THX Deep Note synth -> line-out, DMA-driven)\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    thxApp.tick();

    ++tickCount;
    if ((tickCount % potPrintIntervalTicks) == 0U)
    {
        printPotValues();
    }

    HAL_Delay(appTickIntervalMs);
}
