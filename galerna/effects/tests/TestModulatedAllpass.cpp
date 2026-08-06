#include "galerna/effects/ModulatedAllpass.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>

TEST_CASE("ModulatedAllpass with zero feedback behaves as a plain delay")
{
    galerna::effects::ModulatedAllpass<512U> allpass;
    allpass.init(48'000.0F);
    constexpr float delayS{0.004F};
    allpass.setDelay(delayS, 0.0F, 5.0F);
    allpass.setFeedback(0.0F);

    std::size_t peakIndex{0U};
    float peakValue{0.0F};
    for (std::size_t sample = 0U; sample < 256U; ++sample)
    {
        const float input = sample == 0U ? 1.0F : 0.0F;
        const float value = allpass.process(input);
        if (value > peakValue)
        {
            peakValue = value;
            peakIndex = sample;
        }
    }

    const auto expectedIndex = static_cast<std::size_t>(delayS * 48'000.0F);
    REQUIRE(peakValue > 0.9F);
    REQUIRE(std::abs(static_cast<int>(peakIndex) - static_cast<int>(expectedIndex)) <= 2);
}

TEST_CASE("ModulatedAllpass stays bounded and finite over a long run at a stable feedback")
{
    galerna::effects::ModulatedAllpass<512U> allpass;
    allpass.init(48'000.0F);
    allpass.setDelay(0.005F, 0.001F, 0.2F);
    allpass.setFeedback(0.7F);

    float toggle = 1.0F;
    for (int sample = 0; sample < 48'000; ++sample)
    {
        const float value = allpass.process(toggle);
        toggle = -toggle;
        REQUIRE(std::isfinite(value));
        REQUIRE(std::abs(value) <= 4.0F);
    }
}

TEST_CASE("ModulatedAllpass different feedback settings change the output for the same input")
{
    galerna::effects::ModulatedAllpass<512U> lowFeedback;
    lowFeedback.init(48'000.0F);
    lowFeedback.setDelay(0.005F, 0.0005F, 0.2F);
    lowFeedback.setFeedback(0.2F);

    galerna::effects::ModulatedAllpass<512U> highFeedback;
    highFeedback.init(48'000.0F);
    highFeedback.setDelay(0.005F, 0.0005F, 0.2F);
    highFeedback.setFeedback(0.8F);

    bool everDifferent = false;
    for (int sample = 0; sample < 2'000; ++sample)
    {
        const float input = (sample % 97) == 0 ? 1.0F : 0.0F;
        if (lowFeedback.process(input) != highFeedback.process(input))
        {
            everDifferent = true;
        }
    }
    REQUIRE(everDifferent);
}
