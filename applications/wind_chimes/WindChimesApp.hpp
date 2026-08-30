#pragma once

#include "BoardControls.h"
#include "galerna/core/BinaryLedDisplay.hpp"
#include "galerna/core/DuplexAudioBlockProcessor.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/effects/CloudReverb.hpp"
#include "galerna/effects/WindChimes.hpp"
#include "galerna/platform/stm32f405/Stm32Adc.hpp"
#include "galerna/platform/stm32f405/Stm32Gpio.hpp"
#include "galerna/platform/stm32f405/Stm32I2sDuplexAudio.hpp"

#include "stm32f4xx_hal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace galerna::app
{

// Drives the WindChimes synth demo at control rate: reads 8 PotMux4051 channels into the synth's
// density/spread/decay/timbre/resonance/voiceCount controls plus the CloudReverb chained after it
// (mix/size -- see galerna/effects/CloudReverb.hpp), and mirrors the resulting active voice count
// on the status LEDs in binary (see displayBinary()). Only ever wired up against the real STM32
// GPIO/ADC/mux/I2S peripherals, never against a host fake -- no template parameters needed (same
// reasoning as PotBlinkApp). Owns its status LEDs and the audio engine/effect chain outright (by
// value) since nothing outside this class needs to touch them -- unlike _potMux, which is shared
// with app.cpp's printPotValues() diagnostic and so stays a reference to a longer-lived object.
// Codec bring-up stays a free function in applications/wind_chimes/app.cpp, same reasoning as
// ThxDeepNoteApp -- init() assumes the codec is already configured by the time it's called.
class WindChimesApp
{
public:
    // 64 stereo frames = 2 ms per half at 32 kHz.
    static constexpr std::size_t audioFramesPerHalf{64U};
    using AudioProcessor
        = core::DuplexAudioBlockProcessor<audioFramesPerHalf, effects::WindChimes, effects::CloudReverb>;
    using AudioEngine = platform::stm32f405::Stm32I2sDuplexAudio<audioFramesPerHalf, AudioProcessor>;

    struct PotMuxChannels
    {
        std::uint8_t density;
        std::uint8_t spread;
        std::uint8_t decay;
        std::uint8_t timbre;
        std::uint8_t resonance;
        std::uint8_t voiceCount;
        std::uint8_t reverbMix;
        std::uint8_t reverbSize;
    };

    WindChimesApp(
        std::array<platform::stm32f405::Stm32Gpio, ledCount> statusLeds,
        drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& potMux,
        PotMuxChannels potMuxChannels,
        AudioEngine& audioEngine,
        effects::WindChimes& effect,
        effects::CloudReverb& reverb,
        float sampleRateHz)
        : _statusLeds{std::move(statusLeds)}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _audioEngine{audioEngine}
        , _effect{effect}
        , _reverb{reverb}
        , _sampleRateHz{sampleRateHz}
    {
    }

    // Turns every status LED off. Split out from init() so app.cpp can call it immediately,
    // before codec bring-up, the same way App_Init() always has -- init() itself assumes the
    // codec is already configured.
    void resetStatusLeds()
    {
        for (auto& statusLed : _statusLeds)
        {
            statusLed.set(false);
        }
    }

    // Assumes the codec is already configured (see applications/wind_chimes/app.cpp). Inits the
    // synth and reverb for the real sample rate and starts the DMA-driven audio engine.
    bool init()
    {
        _effect.init(_sampleRateHz);
        _reverb.init(_sampleRateHz);
        return _audioEngine.start();
    }

    // Reads the 8 WindChimes+CloudReverb control pots, applies them, and mirrors the resulting
    // active voice count on the status LEDs. Returns the active voice count applied so the
    // caller can also use it for diagnostics.
    std::size_t tick()
    {
        _effect.setDensity(readNormalized(_potMuxChannels.density));
        _effect.setSpread(readNormalized(_potMuxChannels.spread));
        _effect.setDecay(readNormalized(_potMuxChannels.decay));
        _effect.setTimbre(readNormalized(_potMuxChannels.timbre));
        _effect.setResonance(readNormalized(_potMuxChannels.resonance));
        _reverb.setMix(readNormalized(_potMuxChannels.reverbMix));
        _reverb.setSize(readNormalized(_potMuxChannels.reverbSize));

        const float voiceCountNormalized = readNormalized(_potMuxChannels.voiceCount);
        const auto activeVoiceCount = static_cast<std::size_t>(
            voiceCountNormalized * static_cast<float>(effects::WindChimes::voiceCount) + 0.5F);
        _effect.setActiveVoiceCount(activeVoiceCount);

        std::array<std::reference_wrapper<platform::stm32f405::Stm32Gpio>, ledCount> statusLedRefs{
            std::ref(_statusLeds[0]), std::ref(_statusLeds[1]), std::ref(_statusLeds[2])};
        core::displayBinary(statusLedRefs, static_cast<unsigned>(activeVoiceCount));
        return activeVoiceCount;
    }

private:
    float readNormalized(std::uint8_t muxChannel)
    {
        return static_cast<float>(_potMux.read(muxChannel)) / static_cast<float>(potMaxValue);
    }

    std::array<platform::stm32f405::Stm32Gpio, ledCount> _statusLeds;
    drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& _potMux;
    PotMuxChannels _potMuxChannels;
    AudioEngine& _audioEngine;
    effects::WindChimes& _effect;
    effects::CloudReverb& _reverb;
    float _sampleRateHz;
};

} // namespace galerna::app
