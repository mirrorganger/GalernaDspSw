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

// Drives the ThxDeepNote synth demo at control rate: reads 5 PotMux4051 channels into the
// synth's pitch/pitchShift/timbre/resonance/voiceCount controls and mirrors the resulting
// active voice count on the status LEDs in binary (see displayBinary()). TAudioEngine/TEffect
// are duck-typed (matches galerna/core/ProcessorChain.hpp's style) since they're always the
// single concrete Stm32I2sDuplexAudio<...>/ThxDeepNote instances constructed once per app --
// not swapped for host fakes the way the Adc/Gpio concepts below are. Codec bring-up needs
// HAL_Delay() between register writes, so it stays a free function in
// applications/thx_deep_note/app.cpp (a genuine HAL passthrough, not control logic) rather than
// living here -- init() assumes the codec is already configured by the time it's called.
template <hal::Gpio TStatusLed, hal::Adc TAdc, hal::Gpio TMuxGpio, typename TAudioEngine, typename TEffect>
class ThxDeepNoteApp
{
public:
    static constexpr std::size_t ledCount{3};
    static constexpr std::uint16_t potMaxValue{4'095};

    struct PotMuxChannels
    {
        std::uint8_t pitch;
        std::uint8_t pitchShift;
        std::uint8_t timbre;
        std::uint8_t resonance;
        std::uint8_t voiceCount;
    };

    ThxDeepNoteApp(
        std::array<std::reference_wrapper<TStatusLed>, ledCount> statusLeds,
        drivers::PotMux4051<TAdc, TMuxGpio>& potMux,
        PotMuxChannels potMuxChannels,
        TAudioEngine& audioEngine,
        TEffect& effect,
        float sampleRateHz)
        : _statusLeds{statusLeds}
        , _potMux{potMux}
        , _potMuxChannels{potMuxChannels}
        , _audioEngine{audioEngine}
        , _effect{effect}
        , _sampleRateHz{sampleRateHz}
    {
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
    float _sampleRateHz;
};

} // namespace galerna::app
