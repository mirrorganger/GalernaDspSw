#pragma once

#include "galerna/effects/GrainBuffer.hpp"

#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// One grain-pool slot (see GranularCloud.hpp): inactive until trigger()'d, then reads through a
// shared GrainBuffer at its own position and rate for a fixed duration, enveloped, then goes
// inactive again. No RNG/scheduling of its own -- GranularCloud decides when and how to trigger
// each slot, same "caller decides" split as PluckVoice's noteOn()/noteOff().
//
// playbackRate != 1 changes this grain's read lag relative to the buffer's advancing write
// pointer over its lifetime -- the same delay-modulation-as-pitch-shift technique
// ModulatedDelayLine's LFO wobble already relies on, just driven by a fixed rate instead of an
// LFO. See process().
//
// Envelope: an incrementally-computed parabola (e(t) = 4t(1-t) over the grain's duration, zero at
// both ends, peak 1 at the midpoint), generated via constant-second-difference forward
// differencing -- 2 additions per sample, no table, no trig (Ross Bencina, "Implementing
// Real-Time Granular Synthesis" -- see galerna/effects/README.md's GranularCloud section for the
// link). Deliberately not a lookup table like this codebase's other windows/waveforms
// (WavetableOscillator, ModulatedDelayLine's LFO): a table read here would be a *second*
// interpolated memory read per grain per sample on top of the GrainBuffer read itself, on
// hardware that's already CPU-bound (see GranularCloud.hpp's CPU budget comment).
class Grain
{
public:
    // startDelaySamples: how far back into the buffer's history this grain starts reading (see
    // GrainBuffer::readAt()). playbackRate: 1.0 = normal speed (this grain's read lag stays fixed
    // for its whole life -- see process()'s comment); >1 tape-speeds-up (pitch up), <1 slows down
    // (pitch down). durationSamples: grain length, must be > 0.
    void trigger(float startDelaySamples, float playbackRate, std::uint32_t durationSamples)
    {
        _positionSamples = startDelaySamples;
        _playbackRate = playbackRate;
        _samplesRemaining = durationSamples;

        const auto n = static_cast<float>(durationSamples);
        _envelopeValue = 0.0F;
        _envelopeSlope = 4.0F / n - 4.0F / (n * n);
        _envelopeCurve = -8.0F / (n * n);
        _active = true;
    }

    [[nodiscard]] bool isActive() const
    {
        return _active;
    }

    template <std::size_t BufferSize>
    [[gnu::always_inline]] float process(const GrainBuffer<BufferSize>& buffer)
    {
        if (!_active)
        {
            return 0.0F;
        }

        const float sample = buffer.readAt(_positionSamples) * _envelopeValue;

        // See this class's comment: constant delay at playbackRate 1, shrinking/growing delay
        // (tape speed up/down) otherwise.
        _positionSamples += 1.0F - _playbackRate;
        _envelopeValue += _envelopeSlope;
        _envelopeSlope += _envelopeCurve;

        --_samplesRemaining;
        if (_samplesRemaining == 0U)
        {
            _active = false;
        }

        return sample;
    }

private:
    float _positionSamples{0.0F};
    float _playbackRate{1.0F};
    float _envelopeValue{0.0F};
    float _envelopeSlope{0.0F};
    float _envelopeCurve{0.0F};
    std::uint32_t _samplesRemaining{0U};
    bool _active{false};
};

} // namespace galerna::effects
