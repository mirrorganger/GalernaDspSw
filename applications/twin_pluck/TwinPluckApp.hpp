#pragma once

#include "BoardControls.h"
#include "galerna/core/DuplexAudioBlockProcessor.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/effects/CloudReverb.hpp"
#include "galerna/effects/TwinPluck.hpp"
#include "galerna/platform/stm32f405/Stm32Adc.hpp"
#include "galerna/platform/stm32f405/Stm32Gpio.hpp"
#include "galerna/platform/stm32f405/Stm32I2sDuplexAudio.hpp"

#include "stm32f4xx_hal.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace galerna::app
{

// Drives the TwinPluck instrument at control rate: reads 7 PotMux4051 channels into the two
// button-gated voices' pitch plus the shared decay/timbre/resonance controls and the CloudReverb
// chained after it (mix/size -- see galerna/effects/CloudReverb.hpp), edge-detects the 2 physical
// push buttons and the 2 on/off switches and gates the corresponding voice on/off on each edge
// (sustains for as long as held/on, then releases -- see PluckVoice::noteOn()/noteOff()), and
// mirrors each button voice's ringing state on its own status LED (the switch-gated drone voices
// aren't mirrored on an LED -- a switch's own position already shows whether it's on, unlike a
// momentary button). Only ever wired up against the real STM32 GPIO/ADC/mux/I2S peripherals,
// never against a host fake -- no template parameters needed (same reasoning as PotBlinkApp).
// Owns its status LEDs, buttons, switches, and the audio engine/effect chain outright (by value)
// since nothing outside this class needs to touch them -- unlike _potMux, which is shared with
// app.cpp's printPotValues() diagnostic and so stays a reference to a longer-lived object. Codec
// bring-up stays a free function in applications/twin_pluck/app.cpp, same reasoning as
// WindChimesApp -- init() assumes the codec is already configured by the time it's called.
class TwinPluckApp
{
public:
    // 64 stereo frames = 2 ms per half at 32 kHz.
    static constexpr std::size_t audioFramesPerHalf{64U};
    using AudioProcessor
        = core::DuplexAudioBlockProcessor<audioFramesPerHalf, effects::TwinPluck, effects::CloudReverb>;
    using AudioEngine = platform::stm32f405::Stm32I2sDuplexAudio<audioFramesPerHalf, AudioProcessor>;

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
        std::array<platform::stm32f405::Stm32Gpio, ledCount> statusLeds,
        drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& potMux,
        PotMuxChannels potMuxChannels,
        std::array<platform::stm32f405::Stm32Gpio, buttonCount> buttons,
        std::array<platform::stm32f405::Stm32Gpio, switchCount> switches,
        AudioEngine& audioEngine,
        effects::TwinPluck& effect,
        effects::CloudReverb& reverb,
        float sampleRateHz)
        : _statusLeds{std::move(statusLeds)}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _buttons{std::move(buttons)}
        , _switches{std::move(switches)}
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
        _effect.setVoicePitch(effects::TwinPluck::button1Voice, readNormalized(_potMuxChannels.pitch1));
        _effect.setVoicePitch(effects::TwinPluck::button2Voice, readNormalized(_potMuxChannels.pitch2));
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
            const bool pressed = !_buttons[i].get();
            const std::size_t voice = effects::TwinPluck::button1Voice + i;
            if (pressed && !_previousButtonPressed[i])
            {
                _effect.noteOn(voice);
            }
            else if (!pressed && _previousButtonPressed[i])
            {
                _effect.noteOff(voice);
            }
            _previousButtonPressed[i] = pressed;
            _statusLeds[i].set(_effect.isVoiceRinging(voice));
        }
        // LED2 has no third button voice to mirror -- a direct 1:1 "LED N lit while voice N
        // rings" mapping is more legible here than WindChimes'/pot_blink's
        // core::displayBinary() voice-count encoding, so it's deliberately left off rather than
        // repurposed.
        _statusLeds[2].set(false);

        // Same edge-detection shape as the buttons above, but switches are active-high (on ==
        // get(), same polarity pot_blink's PotBlinkApp already uses for SW1/SW2) and drive the
        // fixed-pitch drone voices instead -- see TwinPluckApp's class comment for why a switch's
        // steady on/off position maps naturally onto the same noteOn()/noteOff() gate a button
        // uses.
        for (std::size_t i = 0U; i < switchCount; ++i)
        {
            const bool on = _switches[i].get();
            const std::size_t voice = effects::TwinPluck::droneRootVoice + i;
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

    std::array<platform::stm32f405::Stm32Gpio, ledCount> _statusLeds;
    drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& _potMux;
    PotMuxChannels _potMuxChannels;
    std::array<platform::stm32f405::Stm32Gpio, buttonCount> _buttons;
    std::array<platform::stm32f405::Stm32Gpio, switchCount> _switches;
    AudioEngine& _audioEngine;
    effects::TwinPluck& _effect;
    effects::CloudReverb& _reverb;
    float _sampleRateHz;
    std::array<bool, buttonCount> _previousButtonPressed{};
    std::array<bool, switchCount> _previousSwitchOn{};
};

} // namespace galerna::app
