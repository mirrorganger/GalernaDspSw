#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace galerna::core
{

// Band-limited sawtooth oscillator, built once as an additive sum of sine harmonics and read
// back through a fixed-size wavetable with linear interpolation between samples.
template <std::size_t TableSize>
class WavetableOscillator
{
public:
    static constexpr std::size_t harmonicCount{48U};

    void init(float sampleRate)
    {
        _inverseSampleRate = 1.0F / sampleRate;
        _readPointer = 0.0F;
        buildSawtoothTable();
    }

    void setFrequency(float frequencyHz)
    {
        _frequencyHz = frequencyHz;
    }

    void setAmplitude(float amplitude)
    {
        _amplitude = amplitude;
    }

    float amplitude() const
    {
        return _amplitude;
    }

    // Real hardware DWT cycle-count profiling showed this hot path costing far more than its
    // handful of FLOPs should -- -Os was declining to inline it across the ThxVoice/ThxDeepNote
    // call chain, so cross-function call overhead (stack push/pop, argument passing) dominated.
    // Forcing inlining here (and in interpolate() below) removes that overhead.
    [[gnu::always_inline]] float process()
    {
        _readPointer += static_cast<float>(TableSize) * _frequencyHz * _inverseSampleRate;
        while (_readPointer >= static_cast<float>(TableSize))
        {
            _readPointer -= static_cast<float>(TableSize);
        }
        while (_readPointer < 0.0F)
        {
            _readPointer += static_cast<float>(TableSize);
        }

        return interpolate() * _amplitude;
    }

private:
    void buildSawtoothTable()
    {
        for (std::size_t sample = 0U; sample < TableSize; ++sample)
        {
            float value{0.0F};
            for (std::size_t harmonic = 1U; harmonic <= harmonicCount; ++harmonic)
            {
                const float phase = 2.0F * std::numbers::pi_v<float> * static_cast<float>(harmonic)
                    * static_cast<float>(sample) / static_cast<float>(TableSize);
                value += std::sin(phase) / static_cast<float>(harmonic);
            }
            _table[sample] = value;
        }
    }

    [[gnu::always_inline]] float interpolate() const
    {
        const auto indexBelow = static_cast<std::size_t>(_readPointer);
        auto indexAbove = indexBelow + 1U;
        if (indexAbove >= TableSize)
        {
            indexAbove = 0U;
        }

        const float weightAbove = _readPointer - static_cast<float>(indexBelow);
        return (1.0F - weightAbove) * _table[indexBelow] + weightAbove * _table[indexAbove];
    }

    std::array<float, TableSize> _table{};
    float _inverseSampleRate{1.0F};
    float _frequencyHz{0.0F};
    float _readPointer{0.0F};
    float _amplitude{0.5F};
};

} // namespace galerna::core
