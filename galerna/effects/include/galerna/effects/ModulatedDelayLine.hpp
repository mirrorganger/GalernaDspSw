#pragma once

#include "galerna/core/WavetableOscillator.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace galerna::effects
{

// Circular delay buffer whose read position wobbles slowly around a base delay time via a
// low-rate LFO, read back with linear interpolation between the two neighboring samples (same
// technique as WavetableOscillator::interpolate()). The wobble is what keeps a diffuser/comb
// network from settling into a static, metallic-sounding fixed delay -- the core ingredient
// behind a "cloudy" rather than plainly resonant reverb tail (see CloudReverb.hpp and
// docs/architecture.md's Reverb section).
template <std::size_t BufferSize>
class ModulatedDelayLine
{
public:
    void init(float sampleRate)
    {
        _sampleRate = sampleRate;
        // Table size 16, not WavetableOscillator's usual 64 (see ThxVoice): this LFO runs far
        // below audio rate (well under 1 Hz here), and CloudReverb instantiates a couple dozen of
        // these, so the smaller table's memory saving is worth more than the (inaudible at this
        // oversampling ratio) extra interpolation error.
        _lfo.init(sampleRate);
        _buffer.fill(0.0F);
        _writeIndex = 0U;
    }

    // baseDelayS/modDepthS: center delay and peak deviation, in seconds. Together they must stay
    // within BufferSize samples at the real init() sample rate -- readDelayed() clamps
    // defensively so this can never read/write out of bounds, but callers should size BufferSize
    // with headroom instead of relying on that clamp to silently truncate the intended delay.
    void setDelay(float baseDelayS, float modDepthS, float modRateHz)
    {
        _baseDelaySamples = baseDelayS * _sampleRate;
        _lfo.setFrequency(modRateHz);
        _lfo.setAmplitude(modDepthS * _sampleRate);
    }

    // always_inline on write()/readDelayed(): real hardware DWT profiling (see
    // docs/architecture.md's Reverb section) showed cross-function call overhead dominating this
    // hot path, the same issue ThxVoice/WavetableOscillator's own always_inline comments describe
    // -- CloudReverb calls into this class up to 24 times per sample.
    [[gnu::always_inline]] void write(float sample)
    {
        _buffer[_writeIndex] = sample;
        _writeIndex = (_writeIndex + 1U) % BufferSize;
    }

    [[gnu::always_inline]] float readDelayed()
    {
        const float delaySamples = std::clamp(
            _baseDelaySamples + _lfo.process(), 1.0F, static_cast<float>(BufferSize - 1U));

        float readPosition = static_cast<float>(_writeIndex) - delaySamples;
        while (readPosition < 0.0F)
        {
            readPosition += static_cast<float>(BufferSize);
        }

        const auto indexBelow = static_cast<std::size_t>(readPosition);
        auto indexAbove = indexBelow + 1U;
        if (indexAbove >= BufferSize)
        {
            indexAbove = 0U;
        }
        const float weightAbove = readPosition - static_cast<float>(indexBelow);
        return (1.0F - weightAbove) * _buffer[indexBelow] + weightAbove * _buffer[indexAbove];
    }

private:
    core::WavetableOscillator<16U> _lfo;
    std::array<float, BufferSize> _buffer{};
    float _sampleRate{1.0F};
    float _baseDelaySamples{1.0F};
    std::size_t _writeIndex{0U};
};

} // namespace galerna::effects
