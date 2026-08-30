#include "app.h"

#include "AudioLoadMonitor.h"
#include "BoardControls.h"
#include "CodecBringup.h"
#include "applications/wind_chimes/WindChimesApp.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/effects/CloudReverb.hpp"
#include "galerna/effects/WindChimes.hpp"

#include "main.h"

#include <cstdio>

extern "C" I2S_HandleTypeDef hi2s2;

namespace
{

galerna::app::BoardControls boardControls{galerna::app::makeBoardControls()};
auto potMux = galerna::app::makePotMux(boardControls);

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t audioLoadReportIntervalTicks{50U}; // ~1 s at appTickIntervalMs

// Generative wind-chime synth -> line-out, driven continuously by DMA (see Stm32I2sDuplexAudio).
// Real line-in capture is currently dead at the hardware level (ADC/ASDOUT stuck high -- see
// docs/progress.md), so WindChimes ignores whatever garbage the DMA rx half contains and
// generates its own signal; the rx half of the DMA transfer keeps running regardless (that's how
// full-duplex I2S works), it's just unused.
// The true I2S sample rate, computed from live register reads (RCC->PLLI2SCFGR: PLLI2SN=50,
// PLLI2SR=2; SPI2->I2SPR: I2SDIV=3, ODD=0, MCKOE=1): I2SCLK = (HSE/PLLM)*PLLI2SN/PLLI2SR =
// (24 MHz/12)*50/2 = 50 MHz; Fs = I2SCLK / (256*(2*I2SDIV+ODD)) = 50 MHz/1536 -- not exactly the
// nominal 32 kHz the ES8388 register script assumes (see docs/progress.md).
constexpr float audioSampleRateHz{50'000'000.0F / 1536.0F};
galerna::effects::WindChimes audioWindChimes;
// CloudReverb ("CloudSeed-lite", see galerna/effects/CloudReverb.hpp and docs/architecture.md's
// Reverb section) chained after the synth -- ProcessorChain runs each processor over the buffer
// in order, so this reverberates WindChimes' output in place rather than generating its own
// signal.
galerna::effects::CloudReverb audioCloudReverb;
galerna::core::ProcessorChain<galerna::effects::WindChimes, galerna::effects::CloudReverb> audioChain{
    audioWindChimes, audioCloudReverb};
galerna::app::WindChimesApp::AudioProcessor audioProcessor{audioChain};
galerna::app::WindChimesApp::AudioEngine audioEngine{hi2s2, audioProcessor};

// WindChimes+CloudReverb controls, one PotMux4051 channel each (all 8 channels are now spoken
// for -- channels 0/4 (POT_3/POT_2) used to be free, now drive the reverb's mix/size).
galerna::app::WindChimesApp windChimesApp{
    boardControls.statusLeds,
    potMux,
    {.density = 1U,
     .spread = 3U,
     .decay = 2U,
     .timbre = 5U,
     .resonance = 7U,
     .voiceCount = 6U,
     .reverbMix = 0U,
     .reverbSize = 4U},
    audioEngine,
    audioWindChimes,
    audioCloudReverb,
    audioSampleRateHz};

} // namespace

extern "C" void App_Init(void)
{
    windChimesApp.resetStatusLeds();

    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));

    if (!galerna::app::runCodecBringup())
    {
        return;
    }

    if (!windChimesApp.init())
    {
        std::printf("I2S duplex audio: HAL_I2SEx_TransmitReceive_DMA failed to start\r\n");
        return;
    }

    std::printf("I2S duplex audio: started (WindChimes synth -> line-out, DMA-driven)\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    windChimesApp.tick();

    ++tickCount;
    if ((tickCount % audioLoadReportIntervalTicks) == 0U)
    {
        galerna::app::printAudioLoad(
            galerna::app::WindChimesApp::audioFramesPerHalf,
            audioSampleRateHz,
            audioEngine.callCount(),
            audioEngine.errorCount(),
            audioEngine.maxProcessCycles(),
            audioProcessor.clipCount());
    }

    HAL_Delay(appTickIntervalMs);
}
