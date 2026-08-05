#pragma once

#include <cstdint>

namespace galerna::core
{

// Minimal xorshift32 PRNG: deterministic (same seed -> same sequence, useful for reproducible
// tests and for giving each generative voice in WindChimes its own decorrelated but repeatable
// stream), fast, and small enough to keep entirely in registers on the audio ISR path. Not
// cryptographically secure -- not needed for scheduling generative note events.
class Xorshift32
{
public:
    // All-zero state is a fixed point (next() would return 0 forever), so a zero seed is
    // remapped to a nonzero default.
    explicit Xorshift32(std::uint32_t seed) : _state{seed != 0U ? seed : 1U}
    {
    }

    std::uint32_t next()
    {
        _state ^= _state << 13U;
        _state ^= _state >> 17U;
        _state ^= _state << 5U;
        return _state;
    }

    // Returns a value in [0, 1).
    float nextFloat01()
    {
        constexpr float inverseRange = 1.0F / 4'294'967'296.0F; // 1 / 2^32
        return static_cast<float>(next()) * inverseRange;
    }

private:
    std::uint32_t _state;
};

} // namespace galerna::core
