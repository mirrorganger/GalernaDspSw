#include "galerna/effects/ModulatedDelayLine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>

TEST_CASE("ModulatedDelayLine reads back silence before anything has been written")
{
    galerna::effects::ModulatedDelayLine<512U> line;
    line.init(48'000.0F);
    line.setDelay(0.005F, 0.0F, 5.0F);

    for (int sample = 0; sample < 200; ++sample)
    {
        REQUIRE(line.readDelayed() == 0.0F);
    }
}

TEST_CASE("ModulatedDelayLine reproduces a written impulse after roughly its delay time")
{
    galerna::effects::ModulatedDelayLine<512U> line;
    line.init(48'000.0F);
    constexpr float delayS{0.005F};
    line.setDelay(delayS, 0.0F, 5.0F); // zero mod depth -> a fixed, predictable delay

    line.write(1.0F);

    std::size_t peakIndex{0U};
    float peakValue{0.0F};
    for (std::size_t sample = 0U; sample < 256U; ++sample)
    {
        const float value = line.readDelayed();
        if (value > peakValue)
        {
            peakValue = value;
            peakIndex = sample;
        }
        line.write(0.0F);
    }

    const auto expectedIndex = static_cast<std::size_t>(delayS * 48'000.0F);
    REQUIRE(peakValue > 0.9F);
    REQUIRE(std::abs(static_cast<int>(peakIndex) - static_cast<int>(expectedIndex)) <= 2);
}

TEST_CASE("ModulatedDelayLine longer delay times push the echoed peak further out")
{
    galerna::effects::ModulatedDelayLine<1'024U> shortDelay;
    shortDelay.init(48'000.0F);
    shortDelay.setDelay(0.003F, 0.0F, 5.0F);

    galerna::effects::ModulatedDelayLine<1'024U> longDelay;
    longDelay.init(48'000.0F);
    longDelay.setDelay(0.015F, 0.0F, 5.0F);

    shortDelay.write(1.0F);
    longDelay.write(1.0F);

    std::size_t shortPeakIndex{0U};
    float shortPeakValue{0.0F};
    std::size_t longPeakIndex{0U};
    float longPeakValue{0.0F};
    for (std::size_t sample = 0U; sample < 1'024U; ++sample)
    {
        const float shortValue = shortDelay.readDelayed();
        if (shortValue > shortPeakValue)
        {
            shortPeakValue = shortValue;
            shortPeakIndex = sample;
        }
        shortDelay.write(0.0F);

        const float longValue = longDelay.readDelayed();
        if (longValue > longPeakValue)
        {
            longPeakValue = longValue;
            longPeakIndex = sample;
        }
        longDelay.write(0.0F);
    }

    REQUIRE(longPeakIndex > shortPeakIndex);
}

TEST_CASE("ModulatedDelayLine stays within the bound of what was written, even with modulation active")
{
    galerna::effects::ModulatedDelayLine<256U> line;
    line.init(48'000.0F);
    line.setDelay(0.002F, 0.001F, 3.0F);

    // A linear interpolation between two buffer samples is a convex combination, so it can never
    // exceed the magnitude of the most extreme value ever written -- true regardless of where the
    // modulated read position lands.
    float toggle = 1.0F;
    for (int sample = 0; sample < 4'000; ++sample)
    {
        line.write(toggle);
        toggle = -toggle;

        const float value = line.readDelayed();
        REQUIRE(std::isfinite(value));
        REQUIRE(std::abs(value) <= 1.0F);
    }
}
