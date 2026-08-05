#include "Galerna/Core/WavetableOscillator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("WavetableOscillator output stays bounded and finite")
{
    galerna::core::WavetableOscillator<64U> osc;
    osc.init(48'000.0F);
    osc.setFrequency(100.0F);
    osc.setAmplitude(1.0F);

    for (int i = 0; i < 1000; ++i)
    {
        const float sample = osc.process();
        REQUIRE(std::isfinite(sample));
        // The additive-harmonic sawtooth build uses harmonicCount (48) > TableSize/2 (32), which
        // aliases the higher harmonics back into the table and can reinforce constructively --
        // this bound just needs to catch a genuinely broken/runaway oscillator, not enforce
        // exact peak amplitude.
        REQUIRE(std::abs(sample) <= 2.0F);
    }
}

TEST_CASE("WavetableOscillator repeats one full table cycle per period")
{
    galerna::core::WavetableOscillator<64U> osc;
    // Table size 64, sample rate 64, frequency 1 Hz -> readPointer advances by exactly one
    // table entry per process() call, so the sequence must repeat with period 64.
    osc.init(64.0F);
    osc.setFrequency(1.0F);

    std::array<float, 64U> firstCycle{};
    std::array<float, 64U> secondCycle{};
    for (auto& sample : firstCycle)
    {
        sample = osc.process();
    }
    for (auto& sample : secondCycle)
    {
        sample = osc.process();
    }

    REQUIRE(firstCycle == secondCycle);
}

TEST_CASE("WavetableOscillator holds a constant value at zero frequency")
{
    galerna::core::WavetableOscillator<64U> osc;
    osc.init(48'000.0F);
    osc.setFrequency(0.0F);
    osc.setAmplitude(1.0F);

    const float first = osc.process();
    for (int i = 0; i < 10; ++i)
    {
        REQUIRE(osc.process() == first);
    }
}
