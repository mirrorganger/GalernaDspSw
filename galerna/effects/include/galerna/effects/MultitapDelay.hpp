#pragma once

#include "galerna/core/Xorshift32.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// Early-reflection block: a single circular buffer read back through TapCount fixed taps at
// randomized (seeded) positions and decaying gains, then summed -- an approximation of a room's
// early echo pattern, the same idea as CloudSeed's "Multitap Delay" block (see
// docs/architecture.md's Reverb section for how this fits into CloudReverb as a whole).
template <std::size_t BufferSize, std::size_t TapCount>
class MultitapDelay
{
public:
    void init(std::uint32_t seed)
    {
        core::Xorshift32 rng{seed};
        _buffer.fill(0.0F);
        _writeIndex = 0U;
        for (std::size_t tap = 0U; tap < TapCount; ++tap)
        {
            // Never tap offset 0 -- that would read back the sample this same process() call is
            // about to overwrite, i.e. no delay at all.
            _tapOffset[tap]
                = 1U + static_cast<std::size_t>(rng.nextFloat01() * static_cast<float>(BufferSize - 1U));
            // Later (further/older) taps get less gain on average, echoing a real room's decaying
            // early-reflection pattern; the per-tap jitter keeps it from sounding like a
            // mechanically even ramp. headroom keeps the summed output roughly in the same range
            // as a single tap regardless of TapCount.
            const float decay = std::pow(tapDecayPerTap, static_cast<float>(tap));
            _tapGain[tap] = decay * (0.5F + rng.nextFloat01() * 0.5F) * headroom;
        }
    }

    // always_inline: see ModulatedDelayLine's comment -- same cross-function call overhead issue.
    [[gnu::always_inline]] float process(float input)
    {
        _buffer[_writeIndex] = input;

        float sum = 0.0F;
        for (std::size_t tap = 0U; tap < TapCount; ++tap)
        {
            const std::size_t readIndex = (_writeIndex + BufferSize - _tapOffset[tap]) % BufferSize;
            sum += _buffer[readIndex] * _tapGain[tap];
        }

        _writeIndex = (_writeIndex + 1U) % BufferSize;
        return sum;
    }

private:
    static constexpr float tapDecayPerTap{0.85F};
    static constexpr float headroom{1.0F / static_cast<float>(TapCount)};

    std::array<float, BufferSize> _buffer{};
    std::array<std::size_t, TapCount> _tapOffset{};
    std::array<float, TapCount> _tapGain{};
    std::size_t _writeIndex{0U};
};

} // namespace galerna::effects
