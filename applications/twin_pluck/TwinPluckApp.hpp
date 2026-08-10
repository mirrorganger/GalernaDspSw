#pragma once

#include "BoardControls.h"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/hal/AdcConcept.hpp"
#include "galerna/hal/GpioConcept.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace galerna::app
{

// Drives the TwinPluck instrument at control rate: reads 7 PotMux4051 channels into the two
// button-gated voices' pitch plus the shared decay/timbre/resonance controls and the CloudReverb
// chained after it (mix/size -- see galerna/effects/CloudReverb.hpp), edge-detects the 2 physical
// push buttons and the 2 on/off switches and gates the corresponding voice on/off on each edge
// (sustains for as long as held/on, then releases -- see PluckVoice::noteOn()/noteOff()), and
// mirrors each button voice's ringing state on its own status LED (the switch-gated drone voices
// aren't mirrored on an LED -- a switch's own position already shows whether it's on, unlike a
// momentary button). Same duck-typed TAudioEngine/TEffect/TReverb shape as WindChimesApp --
// always the single concrete Stm32I2sDuplexAudio<...>/TwinPluck/CloudReverb instances constructed
// once per app, not swapped for host fakes. Codec bring-up stays a free function in
// applications/twin_pluck/app.cpp, same reasoning as WindChimesApp -- init() assumes the codec is
// already configured by the time it's called.
template <
    hal::Gpio TStatusLed,
    hal::Adc TAdc,
    hal::Gpio TMuxGpio,
    hal::Gpio TButton,
    typename TAudioEngine,
    typename TEffect,
    typename TReverb>
class TwinPluckApp
{
public:
    struct PotMuxChannels
    {
        std::uint8_t pitch1;
        std::uint8_t pitch2;
        std::uint8_t decay;
        std::uint8_t timbre;
        std::uint8_t resonance;
        std::uint8_t reverbMix;
        std::uint8_t reverbSize;
    };

    TwinPluckApp(
        std::array<std::reference_wrapper<TStatusLed>, ledCount> statusLeds,
        drivers::PotMux4051<TAdc, TMuxGpio>& potMux,
        PotMuxChannels potMuxChannels,
        std::array<std::reference_wrapper<TButton>, buttonCount> buttons,
        std::array<std::reference_wrapper<TButton>, switchCount> switches,
        TAudioEngine& audioEngine,
        TEffect& effect,
        TReverb& reverb,
        float sampleRateHz)
        : _statusLeds{statusLeds}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{buttons}
        , _switches{switches}
        , _audioEngine{audioEngine}
        , _effect{effect}
        , _reverb{reverb}
        , _sampleRateHz{sampleRateHz}
    {
    }

    // Assumes the codec is already configured (see applications/twin_pluck/app.cpp). Inits the
    // instrument and reverb for the real sample rate and starts the DMA-driven audio engine.
    bool init()
    {
        _effect.init(_sampleRateHz);
        _reverb.init(_sampleRateHz);
        return _audioEngine.start();
    }

    // Reads the 7 TwinPluck+CloudReverb control pots, applies them, edge-detects each button and
    // switch and gates the corresponding voice on/off on each edge, and mirrors each button
    // voice's ringing state onto its own LED.
    void tick()
    {
        _effect.setVoicePitch(TEffect::button1Voice, readNormalized(_potMuxChannels.pitch1));
        _effect.setVoicePitch(TEffect::button2Voice, readNormalized(_potMuxChannels.pitch2));
        _effect.setDecay(readNormalized(_potMuxChannels.decay));
        _effect.setTimbre(readNormalized(_potMuxChannels.timbre));
        _effect.setResonance(readNormalized(_potMuxChannels.resonance));
        _reverb.setMix(readNormalized(_potMuxChannels.reverbMix));
        _reverb.setSize(readNormalized(_potMuxChannels.reverbSize));

        // Edge-detect each button (active-low: pressed == !get()). No debounce/edge-detection
        // infrastructure exists elsewhere in this codebase -- pot_blink's PotBlinkApp only does
        // raw level polling (see its tick()) -- so this is new, deliberately minimal logic: track the
        // previous level and fire noteOn()/noteOff() only on press/release edges, relying on the
        // ~20ms tick period itself as a crude debounce rather than building a general-purpose
        // utility.
        for (std::size_t i = 0U; i < buttonCount; ++i)
        {
            const bool pressed = !_buttons[i].get().get();
            const std::size_t voice = TEffect::button1Voice + i;
            if (pressed && !_previousButtonPressed[i])
            {
                _effect.noteOn(voice);
            }
            else if (!pressed && _previousButtonPressed[i])
            {
                _effect.noteOff(voice);
            }
            _previousButtonPressed[i] = pressed;
            _statusLeds[i].get().set(_effect.isVoiceRinging(voice));
        }
        // LED2 has no third button voice to mirror -- a direct 1:1 "LED N lit while voice N
        // rings" mapping is more legible here than WindChimes'/pot_blink's
        // core::displayBinary() voice-count encoding, so it's deliberately left off rather than
        // repurposed.
        _statusLeds[2].get().set(false);

        // Same edge-detection shape as the buttons above, but switches are active-high (on ==
        // get(), same polarity pot_blink's PotBlinkApp already uses for SW1/SW2) and drive the
        // fixed-pitch drone voices instead -- see TwinPluck's class comment for why a switch's
        // steady on/off position maps naturally onto the same noteOn()/noteOff() gate a button
        // uses.
        for (std::size_t i = 0U; i < switchCount; ++i)
        {
            const bool on = _switches[i].get().get();
            const std::size_t voice = TEffect::droneRootVoice + i;
            if (on && !_previousSwitchOn[i])
            {
                _effect.noteOn(voice);
            }
            else if (!on && _previousSwitchOn[i])
            {
                _effect.noteOff(voice);
            }
            _previousSwitchOn[i] = on;
        }
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
    std::array<std::reference_wrapper<TButton>, switchCount> _switches;
    TAudioEngine& _audioEngine;
    TEffect& _effect;
    TReverb& _reverb;
    float _sampleRateHz;
    std::array<bool, buttonCount> _previousButtonPressed{};
    std::array<bool, switchCount> _previousSwitchOn{};
};

} // namespace galerna::app
