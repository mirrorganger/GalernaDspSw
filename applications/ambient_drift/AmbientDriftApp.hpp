#pragma once

#include "BoardControls.h"
#include "galerna/core/BinaryLedDisplay.hpp"
#include "galerna/core/DuplexAudioBlockProcessor.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/effects/AmbientPad.hpp"
#include "galerna/effects/CloudReverb.hpp"
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

// Drives the AmbientPad generative drone pad at control rate: reads 8 PotMux4051 channels into
// the pad's density/detune/drift-depth/evolve-rate/timbre/resonance controls plus the CloudReverb
// chained after it (mix/size -- see galerna/effects/CloudReverb.hpp), mirrors the resulting active
// voice count on the status LEDs in binary (see displayBinary()), and edge-detects the two push
// buttons into a freeze toggle (BTN1) and a manual reseed trigger (BTN2) -- see AmbientPad's own
// class comment for what those do. Unlike TwinPluck/WindChimes, no voice is ever gated on/off by
// this app: every AmbientPad voice sounds continuously on its own once init()'d. Only ever wired
// up against the real STM32 GPIO/ADC/mux/I2S peripherals, never against a host fake -- no
// template parameters needed (same reasoning as PotBlinkApp). Owns its status LEDs, buttons, and
// the audio engine/effect chain outright (by value) since nothing outside this class needs to
// touch them -- unlike _potMux, which is shared with app.cpp's printPotValues() diagnostic and so
// stays a reference to a longer-lived object. Codec bring-up stays a free function in
// applications/ambient_drift/app.cpp, same reasoning as the other apps -- init() assumes the
// codec is already configured by the time it's called.
class AmbientDriftApp
{
public:
    // 64 stereo frames = 2 ms per half at 32 kHz.
    static constexpr std::size_t audioFramesPerHalf{64U};
    using AudioProcessor
        = core::DuplexAudioBlockProcessor<audioFramesPerHalf, effects::AmbientPad, effects::CloudReverb>;
    using AudioEngine = platform::stm32f405::Stm32I2sDuplexAudio<audioFramesPerHalf, AudioProcessor>;

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
        std::array<platform::stm32f405::Stm32Gpio, ledCount> statusLeds,
        drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& potMux,
        PotMuxChannels potMuxChannels,
        std::array<platform::stm32f405::Stm32Gpio, buttonCount> buttons,
        AudioEngine& audioEngine,
        effects::AmbientPad& effect,
        effects::CloudReverb& reverb,
        float sampleRateHz)
        : _statusLeds{std::move(statusLeds)}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{std::move(buttons)}
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
            densityNormalized * static_cast<float>(effects::AmbientPad::voiceCount) + 0.5F);
        _effect.setActiveVoiceCount(activeVoiceCount);
        std::array<std::reference_wrapper<platform::stm32f405::Stm32Gpio>, ledCount> statusLedRefs{
            std::ref(_statusLeds[0]), std::ref(_statusLeds[1]), std::ref(_statusLeds[2])};
        core::displayBinary(statusLedRefs, static_cast<unsigned>(activeVoiceCount));

        // Edge-detect each button (active-low: pressed == !get()), same crude "tick period as
        // debounce" reasoning as TwinPluckApp::tick() -- no general debounce utility exists
        // elsewhere in this codebase.
        const bool freezePressed = !_buttons[0].get();
        if (freezePressed && !_previousButtonPressed[0])
        {
            _frozen = !_frozen;
            _effect.setFrozen(_frozen);
        }
        _previousButtonPressed[0] = freezePressed;

        const bool reseedPressed = !_buttons[1].get();
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

    std::array<platform::stm32f405::Stm32Gpio, ledCount> _statusLeds;
    drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& _potMux;
    PotMuxChannels _potMuxChannels;
    std::array<platform::stm32f405::Stm32Gpio, buttonCount> _buttons;
    AudioEngine& _audioEngine;
    effects::AmbientPad& _effect;
    effects::CloudReverb& _reverb;
    float _sampleRateHz;
    std::array<bool, buttonCount> _previousButtonPressed{};
    bool _frozen{false};
};

} // namespace galerna::app
