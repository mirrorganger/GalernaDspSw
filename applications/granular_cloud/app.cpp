#include "app.h"

#include "AudioLoadMonitor.h"
#include "BoardControls.h"
#include "CodecBringup.h"
#include "applications/granular_cloud/GranularCloudApp.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/effects/CloudReverb.hpp"
#include "galerna/effects/GranularCloud.hpp"

#include "main.h"

#include <cstdio>

extern "C" I2S_HandleTypeDef hi2s2;

namespace
{

galerna::app::BoardControls boardControls{galerna::app::makeBoardControls()};
auto potMux = galerna::app::makePotMux(boardControls);

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t audioLoadReportIntervalTicks{50U}; // ~1 s at appTickIntervalMs

// Granular-synthesis texture generator -> line-out, driven continuously by DMA (see
// Stm32I2sDuplexAudio). Real line-in capture is currently dead at the hardware level (ADC/ASDOUT
// stuck high -- see docs/progress.md), so GranularCloud ignores whatever garbage the DMA rx half
// contains and generates its own source material to granulate; the rx half of the DMA transfer
// keeps running regardless (that's how full-duplex I2S works), it's just unused.
// The true I2S sample rate, computed from live register reads (RCC->PLLI2SCFGR: PLLI2SN=50,
// PLLI2SR=2; SPI2->I2SPR: I2SDIV=3, ODD=0, MCKOE=1): I2SCLK = (HSE/PLLM)*PLLI2SN/PLLI2SR =
// (24 MHz/12)*50/2 = 50 MHz; Fs = I2SCLK / (256*(2*I2SDIV+ODD)) = 50 MHz/1536 -- not exactly the
// nominal 32 kHz the ES8388 register script assumes (see docs/progress.md).
constexpr float audioSampleRateHz{50'000'000.0F / 1536.0F};
galerna::effects::GranularCloud audioGranularCloud;
// CloudReverb ("CloudSeed-lite", see galerna/effects/CloudReverb.hpp and docs/architecture.md's
// Reverb section) chained after the cloud -- ProcessorChain runs each processor over the buffer
// in order, so this reverberates GranularCloud's output in place rather than generating its own
// signal. Same chaining shape as wind_chimes/twin_pluck/ambient_drift's app.cpp.
// GranularCloud::grainPoolSize settled at 2 after real hardware DWT-cycle-count tuning (started
// at 6, see GranularCloud.hpp's CPU budget comment for the full measurement history) -- re-verify
// with the same method (see galerna::app::printAudioLoad(), called from App_Tick() below) before
// raising it again.
galerna::effects::CloudReverb audioCloudReverb;
galerna::core::ProcessorChain<galerna::effects::GranularCloud, galerna::effects::CloudReverb>
    audioChain{audioGranularCloud, audioCloudReverb};
galerna::app::GranularCloudApp::AudioProcessor audioProcessor{audioChain};
galerna::app::GranularCloudApp::AudioEngine audioEngine{hi2s2, audioProcessor};

// GranularCloud+CloudReverb controls, all 8 PotMux4051 channels spoken for -- no pitch pot needed
// (the source drone's pitch is fixed, see GranularCloud.hpp), so grain size/density/spray/pitch
// spread replace AmbientPad's density/detune/drift depth/evolve rate controls in this app's
// mapping.
galerna::app::GranularCloudApp granularCloudApp{
    boardControls.statusLeds,
    potMux,
    {.grainSize = 0U,
     .density = 1U,
     .spray = 2U,
     .pitchSpread = 3U,
     .timbre = 4U,
     .resonance = 5U,
     .reverbMix = 6U,
     .reverbSize = 7U},
    boardControls.buttons,
    audioEngine,
    audioGranularCloud,
    audioCloudReverb,
    audioSampleRateHz};

} // namespace

extern "C" void App_Init(void)
{
    granularCloudApp.resetStatusLeds();

    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));

    if (!galerna::app::runCodecBringup())
    {
        return;
    }

    if (!granularCloudApp.init())
    {
        std::printf("I2S duplex audio: HAL_I2SEx_TransmitReceive_DMA failed to start\r\n");
        return;
    }

    std::printf("I2S duplex audio: started (GranularCloud synth -> line-out, DMA-driven)\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    granularCloudApp.tick();

    ++tickCount;
    if ((tickCount % audioLoadReportIntervalTicks) == 0U)
    {
        galerna::app::printAudioLoad(
            galerna::app::GranularCloudApp::audioFramesPerHalf,
            audioSampleRateHz,
            audioEngine.callCount(),
            audioEngine.errorCount(),
            audioEngine.maxProcessCycles(),
            audioProcessor.clipCount());
    }

    HAL_Delay(appTickIntervalMs);
}
