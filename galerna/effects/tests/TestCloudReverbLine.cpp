#include "galerna/effects/CloudReverbLine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
using TestLine = galerna::effects::CloudReverbLine<1'152U, 512U, 2U>;
}

TEST_CASE("CloudReverbLine is silent while only ever fed silence")
{
    TestLine line;
    line.init(48'000.0F, 0.020F, 0.002F, 0.1F);
    line.setFeedback(0.8F);

    for (int sample = 0; sample < 4'000; ++sample)
    {
        REQUIRE(line.process(0.0F) == 0.0F);
    }
}

TEST_CASE("CloudReverbLine output stays bounded and finite at maximum feedback")
{
    TestLine line;
    line.init(48'000.0F, 0.020F, 0.002F, 0.1F);
    line.setFeedback(1.0F); // clamped internally to CloudReverbLine's own maxFeedback

    float toggle = 1.0F;
    for (int sample = 0; sample < 48'000; ++sample)
    {
        const float value = line.process(toggle * 0.5F);
        toggle = -toggle;
        REQUIRE(std::isfinite(value));
        REQUIRE(std::abs(value) <= 8.0F);
    }
}

TEST_CASE("CloudReverbLine higher feedback sustains energy longer after the input stops")
{
    TestLine lowFeedback;
    lowFeedback.init(48'000.0F, 0.020F, 0.002F, 0.1F);
    lowFeedback.setFeedback(0.2F);

    TestLine highFeedback;
    highFeedback.init(48'000.0F, 0.020F, 0.002F, 0.1F);
    highFeedback.setFeedback(0.9F);

    // Feed an identical short burst into both, then silence.
    for (int sample = 0; sample < 200; ++sample)
    {
        lowFeedback.process(1.0F);
        highFeedback.process(1.0F);
    }

    float lowTailEnergy{0.0F};
    float highTailEnergy{0.0F};
    constexpr int silenceSamples{8'000};
    constexpr int tailWindow{500};
    for (int sample = 0; sample < silenceSamples; ++sample)
    {
        const float lowValue = lowFeedback.process(0.0F);
        const float highValue = highFeedback.process(0.0F);
        if (sample >= silenceSamples - tailWindow)
        {
            lowTailEnergy += std::abs(lowValue);
            highTailEnergy += std::abs(highValue);
        }
    }

    REQUIRE(highTailEnergy > lowTailEnergy);
}
