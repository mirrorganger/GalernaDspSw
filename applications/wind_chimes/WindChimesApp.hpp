#pragma once

#include "galerna/core/BinaryLedDisplay.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/hal/AdcConcept.hpp"
#include "galerna/hal/GpioConcept.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace galerna::app
{

// Drives the WindChimes synth demo at control rate: reads 8 PotMux4051 channels into the synth's
// density/spread/decay/timbre/resonance/voiceCount controls plus the CloudReverb chained after it
// (mix/size -- see galerna/effects/CloudReverb.hpp), and mirrors the resulting active voice count
// on the status LEDs in binary (see displayBinary()). Same shape as ThxDeepNoteApp --
// TAudioEngine/TEffect/TReverb are duck-typed for the same reason (always the single concrete
// Stm32I2sDuplexAudio<...>/WindChimes/CloudReverb instances constructed once per app, not swapped
// for host fakes). Codec bring-up stays a free function in applications/wind_chimes/app.cpp, same
// reasoning as ThxDeepNoteApp -- init() assumes the codec is already configured by the time it's
// called.
template <
    hal::Gpio TStatusLed,
    hal::Adc TAdc,
    hal::Gpio TMuxGpio,
    typename TAudioEngine,
    typename TEffect,
    typename TReverb>
class WindChimesApp
{
public:
    static constexpr std::size_t ledCount{3};
    static constexpr std::uint16_t potMaxValue{4'095};

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
        std::array<std::reference_wrapper<TStatusLed>, ledCount> statusLeds,
        drivers::PotMux4051<TAdc, TMuxGpio>& potMux,
        PotMuxChannels potMuxChannels,
        TAudioEngine& audioEngine,
        TEffect& effect,
        TReverb& reverb,
        float sampleRateHz)
        : _statusLeds{statusLeds}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _audioEngine{audioEngine}
        , _effect{effect}
        , _reverb{reverb}
        , _sampleRateHz{sampleRateHz}
    {
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
            voiceCountNormalized * static_cast<float>(TEffect::voiceCount) + 0.5F);
        _effect.setActiveVoiceCount(activeVoiceCount);

        core::displayBinary(_statusLeds, static_cast<unsigned>(activeVoiceCount));
        return activeVoiceCount;
    }

private:
    float readNormalized(std::uint8_t muxChannel)
    {
        return static_cast<float>(_potMux.read(muxChannel)) / static_cast<float>(potMaxValue);
    }

    std::array<std::reference_wrapper<TStatusLed>, ledCount> _statusLeds;
    drivers::PotMux4051<TAdc, TMuxGpio>& _potMux;
    PotMuxChannels _potMuxChannels;
    TAudioEngine& _audioEngine;
    TEffect& _effect;
    TReverb& _reverb;
    float _sampleRateHz;
};

} // namespace galerna::app
