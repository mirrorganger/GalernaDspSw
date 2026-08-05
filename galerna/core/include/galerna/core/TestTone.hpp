#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace galerna::core
{

class TestTone
{
public:
    explicit TestTone(std::int16_t amplitude)
        : amplitude_{amplitude}
    {
    }

    void fillStereo(std::span<std::int16_t> interleavedStereo)
    {
        for (auto sample = std::size_t{}; sample + 1U < interleavedStereo.size(); sample += 2U)
        {
            const auto value = nextSample();
            interleavedStereo[sample] = value;
            interleavedStereo[sample + 1U] = value;
        }
    }

private:
    std::int16_t nextSample()
    {
        constexpr auto wavetableAmplitude = std::int32_t{4'096};
        const auto scaled = (static_cast<std::int32_t>(sineWave_[phase_]) * amplitude_) / wavetableAmplitude;
        phase_ = (phase_ + 1U) % sineWave_.size();
        return static_cast<std::int16_t>(scaled);
    }

    static constexpr std::array<std::int16_t, 32> sineWave_{
        0, 799, 1567, 2276, 2896, 3406, 3784, 4017,
        4096, 4017, 3784, 3406, 2896, 2276, 1567, 799,
        0, -799, -1567, -2276, -2896, -3406, -3784, -4017,
        -4096, -4017, -3784, -3406, -2896, -2276, -1567, -799,
    };

    std::size_t phase_{};
    std::int16_t amplitude_{};
};

} // namespace galerna::core
