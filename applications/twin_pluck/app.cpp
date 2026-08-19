#include "app.h"

#include "AudioLoadMonitor.h"
#include "BoardControls.h"
#include "CodecBringup.h"
#include "applications/twin_pluck/TwinPluckApp.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/effects/CloudReverb.hpp"
#include "galerna/effects/TwinPluck.hpp"

#include "main.h"

#include <cstdio>

extern "C" I2S_HandleTypeDef hi2s2;

namespace
{

galerna::app::BoardControls boardControls{galerna::app::makeBoardControls()};
auto potMux = galerna::app::makePotMux(boardControls);

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t audioLoadReportIntervalTicks{50U}; // ~1 s at appTickIntervalMs

// Twin plucked-oscillator instrument -> line-out, driven continuously by DMA (see
// Stm32I2sDuplexAudio). Real line-in capture is currently dead at the hardware level (ADC/ASDOUT
// stuck high -- see docs/progress.md), so TwinPluck ignores whatever garbage the DMA rx half
// contains and generates its own signal; the rx half of the DMA transfer keeps running regardless
// (that's how full-duplex I2S works), it's just unused.
// The true I2S sample rate, computed from live register reads (RCC->PLLI2SCFGR: PLLI2SN=50,
// PLLI2SR=2; SPI2->I2SPR: I2SDIV=3, ODD=0, MCKOE=1): I2SCLK = (HSE/PLLM)*PLLI2SN/PLLI2SR =
// (24 MHz/12)*50/2 = 50 MHz; Fs = I2SCLK / (256*(2*I2SDIV+ODD)) = 50 MHz/1536 -- not exactly the
// nominal 32 kHz the ES8388 register script assumes (see docs/progress.md).
constexpr float audioSampleRateHz{50'000'000.0F / 1536.0F};
galerna::effects::TwinPluck audioTwinPluck;
// CloudReverb ("CloudSeed-lite", see galerna/effects/CloudReverb.hpp and docs/architecture.md's
// Reverb section) chained after the instrument -- ProcessorChain runs each processor over the
// buffer in order, so this reverberates TwinPluck's output in place rather than generating its
// own signal. Same chaining shape as wind_chimes/app.cpp; hardware-verified at 82.35% of the
// per-block CPU budget with zero clipping at TwinPluck's full 4-voice size, including the drone
// pitch wander (see TwinPluck.hpp's class comment for the full measurement).
galerna::effects::CloudReverb audioCloudReverb;
galerna::core::ProcessorChain<galerna::effects::TwinPluck, galerna::effects::CloudReverb> audioChain{
    audioTwinPluck, audioCloudReverb};
galerna::app::TwinPluckApp::AudioProcessor audioProcessor{audioChain};
galerna::app::TwinPluckApp::AudioEngine audioEngine{hi2s2, audioProcessor};

// TwinPluck+CloudReverb controls, 7 of the 8 PotMux4051 channels (channel 7 / POT_8 is spare --
// the two drone voices gated by SW1/SW2 have a fixed pitch, see TwinPluck::init(), so they don't
// need a pot).
galerna::app::TwinPluckApp twinPluckApp{
    boardControls.statusLeds,
    potMux,
    {.pitch1 = 0U,
     .pitch2 = 1U,
     .decay = 2U,
     .timbre = 3U,
     .resonance = 4U,
     .reverbMix = 5U,
     .reverbSize = 6U},
    boardControls.buttons,
    boardControls.switches,
    audioEngine,
    audioTwinPluck,
    audioCloudReverb,
    audioSampleRateHz};

} // namespace

extern "C" void App_Init(void)
{
    twinPluckApp.resetStatusLeds();

    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));

    if (!galerna::app::runCodecBringup())
    {
        return;
    }

    if (!twinPluckApp.init())
    {
        std::printf("I2S duplex audio: HAL_I2SEx_TransmitReceive_DMA failed to start\r\n");
        return;
    }

    std::printf("I2S duplex audio: started (TwinPluck synth -> line-out, DMA-driven)\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    twinPluckApp.tick();

    ++tickCount;
    if ((tickCount % audioLoadReportIntervalTicks) == 0U)
    {
        galerna::app::printAudioLoad(
            galerna::app::TwinPluckApp::audioFramesPerHalf,
            audioSampleRateHz,
            audioEngine.callCount(),
            audioEngine.errorCount(),
            audioEngine.maxProcessCycles(),
            audioProcessor.clipCount());
    }

    HAL_Delay(appTickIntervalMs);
}
