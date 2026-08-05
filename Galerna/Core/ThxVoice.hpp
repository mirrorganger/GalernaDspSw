#pragma once

#include "Galerna/Core/WavetableOscillator.hpp"

#include <cstddef>

namespace galerna::core
{

// One voice of a Deep Note-style synth: a sawtooth oscillator whose frequency glides between a
// shared low "scattered" frequency and its own target frequency, plus a slow LFO applied as
// pitch wobble/vibrato whose depth fades in as the glide approaches the target.
class ThxVoice
{
public:
    static constexpr float lowestFrequencyHz{36.0F};
    static constexpr float highestFrequencyHz{1500.0F};

    // The LFO wobbles at 5-30 Hz, far below audio rate, so evaluating it once every
    // lfoUpdateStride samples instead of every sample is inaudible (still >100x oversampled
    // relative to the LFO rate) but roughly halves this voice's oscillator-evaluation cost --
    // real hardware measurement showed the audio ISR needs that headroom (see docs/progress.md).
    static constexpr std::size_t lfoUpdateStride{4U};

    void init(float sampleRate, float targetFrequencyHz, float lfoRateHz)
    {
        _osc.init(sampleRate);
        _lfo.init(sampleRate / static_cast<float>(lfoUpdateStride));
        _lfo.setFrequency(lfoRateHz);
        _targetFrequencyHz = targetFrequencyHz;
        _oscFrequencyHz = lowestFrequencyHz;
    }

    // pitch: 0..1, glides the oscillator frequency from (lowestFrequencyHz + pitchShiftHz) at 0
    // to (targetFrequencyHz + pitchShiftHz) at 1. pitchShiftHz is an already-scaled additive Hz
    // offset applied to both ends of the glide range.
    void update(float pitch, float pitchShiftHz)
    {
        const float glideMin = lowestFrequencyHz + pitchShiftHz;
        const float glideMax = _targetFrequencyHz + pitchShiftHz;
        _oscFrequencyHz = glideMin + pitch * (glideMax - glideMin);
        _lfo.setAmplitude(pitch * lfoMaxAmplitude);
    }

    [[gnu::always_inline]] float process()
    {
        if (_lfoSampleCounter == 0U)
        {
            _lfoValue = _lfo.process();
        }
        _lfoSampleCounter = (_lfoSampleCounter + 1U) % lfoUpdateStride;

        _osc.setFrequency(_oscFrequencyHz * (1.0F + _lfoValue));
        return _osc.process();
    }

    float frequencyHz() const
    {
        return _oscFrequencyHz;
    }

    float lfoAmplitude() const
    {
        return _lfo.amplitude();
    }

private:
    static constexpr float lfoMaxAmplitude{0.005F};

    WavetableOscillator<64U> _osc;
    WavetableOscillator<64U> _lfo;
    float _targetFrequencyHz{highestFrequencyHz};
    float _oscFrequencyHz{lowestFrequencyHz};
    float _lfoValue{0.0F};
    std::size_t _lfoSampleCounter{0U};
};

} // namespace galerna::core
