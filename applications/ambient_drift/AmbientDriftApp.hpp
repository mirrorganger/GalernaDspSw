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

// Drives the AmbientPad generative drone pad at control rate: reads 8 PotMux4051 channels into
// the pad's density/detune/drift-depth/evolve-rate/timbre/resonance controls plus the CloudReverb
// chained after it (mix/size -- see galerna/effects/CloudReverb.hpp), mirrors the resulting active
// voice count on the status LEDs in binary (see displayBinary()), and edge-detects the two push
// buttons into a freeze toggle (BTN1) and a manual reseed trigger (BTN2) -- see AmbientPad's own
// class comment for what those do. Unlike TwinPluck/WindChimes, no voice is ever gated on/off by
// this app: every AmbientPad voice sounds continuously on its own once init()'d. Same
// TAudioEngine/TEffect/TReverb duck-typed shape as WindChimesApp/TwinPluckApp -- always the single
// concrete Stm32I2sDuplexAudio<...>/AmbientPad/CloudReverb instances constructed once per app, not
// swapped for host fakes. Codec bring-up stays a free function in applications/ambient_drift/app.cpp,
// same reasoning as the other apps -- init() assumes the codec is already configured by the time
// it's called.
template <
    hal::Gpio TStatusLed,
    hal::Adc TAdc,
    hal::Gpio TMuxGpio,
    hal::Gpio TButton,
    typename TAudioEngine,
    typename TEffect,
    typename TReverb>
class AmbientDriftApp
{
public:
    static constexpr std::size_t ledCount{3};
    static constexpr std::size_t buttonCount{2};
    static constexpr std::uint16_t potMaxValue{4'095};

    struct PotMuxChannels
    {
        std::uint8_t density;
        std::uint8_t detune;
        std::uint8_t driftDepth;
        std::uint8_t evolveRate;
        std::uint8_t timbre;
        std::uint8_t resonance;
        std::uint8_t reverbMix;
        std::uint8_t reverbSize;
    };

    AmbientDriftApp(
        std::array<std::reference_wrapper<TStatusLed>, ledCount> statusLeds,
        drivers::PotMux4051<TAdc, TMuxGpio>& potMux,
        PotMuxChannels potMuxChannels,
        std::array<std::reference_wrapper<TButton>, buttonCount> buttons,
        TAudioEngine& audioEngine,
        TEffect& effect,
        TReverb& reverb,
        float sampleRateHz)
        : _statusLeds{statusLeds}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{buttons}
        , _audioEngine{audioEngine}
        , _effect{effect}
        , _reverb{reverb}
        , _sampleRateHz{sampleRateHz}
    {
    }

    // Assumes the codec is already configured (see applications/ambient_drift/app.cpp). Inits the
    // pad and reverb for the real sample rate and starts the DMA-driven audio engine.
    bool init()
    {
        _effect.init(_sampleRateHz);
        _reverb.init(_sampleRateHz);
        return _audioEngine.start();
    }

    // Reads the 8 AmbientPad+CloudReverb control pots, applies them, mirrors the resulting active
    // voice count on the status LEDs, and edge-detects BTN1 (freeze toggle)/BTN2 (reseed trigger).
    // Returns the active voice count applied so the caller can also use it for diagnostics.
    std::size_t tick()
    {
        _effect.setDetune(readNormalized(_potMuxChannels.detune));
        _effect.setDriftDepth(readNormalized(_potMuxChannels.driftDepth));
        _effect.setEvolveRate(readNormalized(_potMuxChannels.evolveRate));
        _effect.setTimbre(readNormalized(_potMuxChannels.timbre));
        _effect.setResonance(readNormalized(_potMuxChannels.resonance));
        _reverb.setMix(readNormalized(_potMuxChannels.reverbMix));
        _reverb.setSize(readNormalized(_potMuxChannels.reverbSize));

        const float densityNormalized = readNormalized(_potMuxChannels.density);
        const auto activeVoiceCount = static_cast<std::size_t>(
            densityNormalized * static_cast<float>(TEffect::voiceCount) + 0.5F);
        _effect.setActiveVoiceCount(activeVoiceCount);
        core::displayBinary(_statusLeds, static_cast<unsigned>(activeVoiceCount));

        // Edge-detect each button (active-low: pressed == !get()), same crude "tick period as
        // debounce" reasoning as TwinPluckApp::tick() -- no general debounce utility exists
        // elsewhere in this codebase.
        const bool freezePressed = !_buttons[0].get().get();
        if (freezePressed && !_previousButtonPressed[0])
        {
            _frozen = !_frozen;
            _effect.setFrozen(_frozen);
        }
        _previousButtonPressed[0] = freezePressed;

        const bool reseedPressed = !_buttons[1].get().get();
        if (reseedPressed && !_previousButtonPressed[1])
        {
            _effect.reseed();
        }
        _previousButtonPressed[1] = reseedPressed;

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
    std::array<std::reference_wrapper<TButton>, buttonCount> _buttons;
    TAudioEngine& _audioEngine;
    TEffect& _effect;
    TReverb& _reverb;
    float _sampleRateHz;
    std::array<bool, buttonCount> _previousButtonPressed{};
    bool _frozen{false};
};

} // namespace galerna::app
