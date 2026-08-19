#pragma once

#include "BoardControls.h"
#include "galerna/core/BinaryLedDisplay.hpp"
#include "galerna/core/DuplexAudioBlockProcessor.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "galerna/effects/ThxDeepNote.hpp"
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

// Drives the ThxDeepNote synth demo at control rate: reads 5 PotMux4051 channels into the
// synth's pitch/pitchShift/timbre/resonance/voiceCount controls and mirrors the resulting
// active voice count on the status LEDs in binary (see displayBinary()). Only ever wired up
// against the real STM32 GPIO/ADC/mux/I2S peripherals, never against a host fake -- no template
// parameters needed (same reasoning as PotBlinkApp). Owns its status LEDs and the audio
// engine/effect chain outright (by value) since nothing outside this class needs to touch them
// -- unlike _potMux, which is shared with app.cpp's printPotValues() diagnostic and so stays a
// reference to a longer-lived object. Codec bring-up needs HAL_Delay() between register writes,
// so it stays a free function in applications/thx_deep_note/app.cpp (a genuine HAL passthrough,
// not control logic) rather than living here -- init() assumes the codec is already configured
// by the time it's called.
class ThxDeepNoteApp
{
public:
    // 64 stereo frames = 2 ms per half at 32 kHz.
    static constexpr std::size_t audioFramesPerHalf{64U};
    using AudioProcessor = core::DuplexAudioBlockProcessor<audioFramesPerHalf, effects::ThxDeepNote>;
    using AudioEngine = platform::stm32f405::Stm32I2sDuplexAudio<audioFramesPerHalf, AudioProcessor>;

    struct PotMuxChannels
    {
        std::uint8_t pitch;
        std::uint8_t pitchShift;
        std::uint8_t timbre;
        std::uint8_t resonance;
        std::uint8_t voiceCount;
    };

    ThxDeepNoteApp(
        std::array<platform::stm32f405::Stm32Gpio, ledCount> statusLeds,
        drivers::PotMux4051<platform::stm32f405::Stm32Adc, platform::stm32f405::Stm32Gpio>& potMux,
        PotMuxChannels potMuxChannels,
        AudioEngine& audioEngine,
        effects::ThxDeepNote& effect,
        float sampleRateHz)
        : _statusLeds{std::move(statusLeds)}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _audioEngine{audioEngine}
        , _effect{effect}
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

    // Assumes the codec is already configured (see applications/thx_deep_note/app.cpp). Inits
    // the synth for the real sample rate and starts the DMA-driven audio engine.
    bool init()
    {
        _effect.init(_sampleRateHz);
        return _audioEngine.start();
    }

    // Reads the 5 THX control pots, applies them to the effect, and mirrors the resulting
    // active voice count on the status LEDs. Returns the active voice count applied so the
    // caller can also use it for diagnostics.
    std::size_t tick()
    {
        _effect.setPitch(readNormalized(_potMuxChannels.pitch));
        _effect.setPitchShift(readNormalized(_potMuxChannels.pitchShift));
        _effect.setTimbre(readNormalized(_potMuxChannels.timbre));
        _effect.setResonance(readNormalized(_potMuxChannels.resonance));

        const float voiceCountNormalized = readNormalized(_potMuxChannels.voiceCount);
        const auto activeVoiceCount = static_cast<std::size_t>(
            voiceCountNormalized * static_cast<float>(effects::ThxDeepNote::voiceCount) + 0.5F);
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
    effects::ThxDeepNote& _effect;
    float _sampleRateHz;
};

} // namespace galerna::app
