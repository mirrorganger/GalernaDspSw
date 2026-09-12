#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace galerna::effects
{

// Plain circular audio buffer: continuous write(), and readAt() for an externally supplied
// fractional offset -- unlike ModulatedDelayLine (which owns its own LFO and always reads at
// "now minus its own wobbling delay"), several independent Grains each need to read this same
// shared buffer at their own, independently-advancing offset (see GranularCloud.hpp). Same
// linear-interpolation technique as ModulatedDelayLine::readDelayed()/
// WavetableOscillator::interpolate().
template <std::size_t BufferSize>
class GrainBuffer
{
public:
    void init()
    {
        _buffer.fill(0.0F);
        _writeIndex = 0U;
    }

    [[gnu::always_inline]] void write(float sample)
    {
        _buffer[_writeIndex] = sample;
        _writeIndex = (_writeIndex + 1U) % BufferSize;
    }

    // delaySamples: how far back from the current write position to read, clamped into
    // [0, BufferSize - 1] so a caller with a stale/out-of-range offset (e.g. a Grain whose
    // position drifted from a large playback-rate deviation over a long duration) can never read
    // out of bounds -- it just reads the oldest/newest sample available instead.
    [[gnu::always_inline]] float readAt(float delaySamples) const
    {
        const float clampedDelay
            = std::clamp(delaySamples, 0.0F, static_cast<float>(BufferSize - 1U));

        // _writeIndex already points at the *next* free slot after write() -- offset by 1 so
        // delay 0 lands on the most recently written sample, not the stale slot about to be
        // overwritten.
        float readPosition = static_cast<float>(_writeIndex) - 1.0F - clampedDelay;
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
    std::array<float, BufferSize> _buffer{};
    std::size_t _writeIndex{0U};
};

} // namespace galerna::effects
