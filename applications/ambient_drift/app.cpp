#include "app.h"

#include "AudioLoadMonitor.h"
#include "BoardControls.h"
#include "CodecBringup.h"
#include "applications/ambient_drift/AmbientDriftApp.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/effects/AmbientPad.hpp"
#include "galerna/effects/CloudReverb.hpp"

#include "main.h"

#include <cstdio>

extern "C" I2S_HandleTypeDef hi2s2;

namespace
{

galerna::app::BoardControls boardControls{galerna::app::makeBoardControls()};
auto potMux = galerna::app::makePotMux(boardControls);

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t audioLoadReportIntervalTicks{50U}; // ~1 s at appTickIntervalMs

// Generative ambient drone pad -> line-out, driven continuously by DMA (see
// Stm32I2sDuplexAudio). Real line-in capture is currently dead at the hardware level (ADC/ASDOUT
// stuck high -- see docs/progress.md), so AmbientPad ignores whatever garbage the DMA rx half
// contains and generates its own signal; the rx half of the DMA transfer keeps running regardless
// (that's how full-duplex I2S works), it's just unused.
// The true I2S sample rate, computed from live register reads (RCC->PLLI2SCFGR: PLLI2SN=50,
// PLLI2SR=2; SPI2->I2SPR: I2SDIV=3, ODD=0, MCKOE=1): I2SCLK = (HSE/PLLM)*PLLI2SN/PLLI2SR =
// (24 MHz/12)*50/2 = 50 MHz; Fs = I2SCLK / (256*(2*I2SDIV+ODD)) = 50 MHz/1536 -- not exactly the
// nominal 32 kHz the ES8388 register script assumes (see docs/progress.md).
constexpr float audioSampleRateHz{50'000'000.0F / 1536.0F};
galerna::effects::AmbientPad audioAmbientPad;
// CloudReverb ("CloudSeed-lite", see galerna/effects/CloudReverb.hpp and docs/architecture.md's
// Reverb section) chained after the pad -- ProcessorChain runs each processor over the buffer in
// order, so this reverberates AmbientPad's output in place rather than generating its own signal.
// Same chaining shape as wind_chimes/twin_pluck's app.cpp. AmbientPad::voiceCount starts at 3
// (WindChimes' hardware-proven-safe count) specifically to stay in that same CPU-budget range --
// re-verify with the DWT cycle-count method (see galerna::app::printAudioLoad(), called from
// App_Tick() below) before raising it.
galerna::effects::CloudReverb audioCloudReverb;
galerna::core::ProcessorChain<galerna::effects::AmbientPad, galerna::effects::CloudReverb> audioChain{
    audioAmbientPad, audioCloudReverb};
galerna::app::AmbientDriftApp::AudioProcessor audioProcessor{audioChain};
galerna::app::AmbientDriftApp::AudioEngine audioEngine{hi2s2, audioProcessor};

// AmbientPad+CloudReverb controls, all 8 PotMux4051 channels spoken for -- no pitch pots needed
// (every voice self-drifts, see AmbientPad.hpp), so density/detune/drift depth/evolve rate replace
// TwinPluck's pitch1/pitch2/decay controls in this app's mapping.
galerna::app::AmbientDriftApp ambientDriftApp{
    boardControls.statusLeds,
    potMux,
    {.density = 0U,
     .detune = 1U,
     .driftDepth = 2U,
     .evolveRate = 3U,
     .timbre = 4U,
     .resonance = 5U,
     .reverbMix = 6U,
     .reverbSize = 7U},
    boardControls.buttons,
    audioEngine,
    audioAmbientPad,
    audioCloudReverb,
    audioSampleRateHz};

} // namespace

extern "C" void App_Init(void)
{
    ambientDriftApp.resetStatusLeds();

    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));

    if (!galerna::app::runCodecBringup())
    {
        return;
    }

    if (!ambientDriftApp.init())
    {
        std::printf("I2S duplex audio: HAL_I2SEx_TransmitReceive_DMA failed to start\r\n");
        return;
    }

    std::printf("I2S duplex audio: started (AmbientPad synth -> line-out, DMA-driven)\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    ambientDriftApp.tick();

    ++tickCount;
    if ((tickCount % audioLoadReportIntervalTicks) == 0U)
    {
        galerna::app::printAudioLoad(
            galerna::app::AmbientDriftApp::audioFramesPerHalf,
            audioSampleRateHz,
            audioEngine.callCount(),
            audioEngine.errorCount(),
            audioEngine.maxProcessCycles(),
            audioProcessor.clipCount());
    }

    HAL_Delay(appTickIntervalMs);
}
