#include "galerna/effects/MultitapDelay.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("MultitapDelay is silent while only ever fed silence")
{
    galerna::effects::MultitapDelay<512U, 8U> multitap;
    multitap.init(1U);

    for (int sample = 0; sample < 600; ++sample)
    {
        REQUIRE(multitap.process(0.0F) == 0.0F);
    }
}

TEST_CASE("MultitapDelay output stays bounded and finite for bounded input")
{
    galerna::effects::MultitapDelay<512U, 12U> multitap;
    multitap.init(2U);

    float toggle = 1.0F;
    for (int sample = 0; sample < 4'000; ++sample)
    {
        const float value = multitap.process(toggle);
        toggle = -toggle;
        REQUIRE(std::isfinite(value));
        REQUIRE(std::abs(value) <= 1.5F);
    }
}

TEST_CASE("MultitapDelay different seeds produce different tap patterns")
{
    galerna::effects::MultitapDelay<512U, 12U> first;
    first.init(3U);

    galerna::effects::MultitapDelay<512U, 12U> second;
    second.init(4U);

    bool everDifferent = false;
    for (int sample = 0; sample < 512; ++sample)
    {
        const float input = (sample % 37) == 0 ? 1.0F : 0.0F;
        if (first.process(input) != second.process(input))
        {
            everDifferent = true;
        }
    }
    REQUIRE(everDifferent);
}

TEST_CASE("MultitapDelay with the same seed reproduces the same output")
{
    galerna::effects::MultitapDelay<512U, 12U> first;
    first.init(42U);

    galerna::effects::MultitapDelay<512U, 12U> second;
    second.init(42U);

    for (int sample = 0; sample < 512; ++sample)
    {
        const float input = (sample % 29) == 0 ? 1.0F : 0.0F;
        REQUIRE(first.process(input) == second.process(input));
    }
}
