#include "galerna/core/AudioBuffer.hpp"
#include "galerna/effects/WindChimes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("WindChimes is silent with zero active voices")
{
    galerna::effects::WindChimes chimes;
    chimes.init(48'000.0F);
    chimes.setActiveVoiceCount(0U);

    std::array<float, 32> left{};
    std::array<float, 32> right{};
    left.fill(1.0F);
    right.fill(1.0F);
    galerna::core::AudioBuffer buffer{left, right};

    chimes.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(sample == 0.0F);
    }
    for (float sample : right)
    {
        REQUIRE(sample == 0.0F);
    }
}

TEST_CASE("WindChimes output stays bounded and finite at full voice count")
{
    galerna::effects::WindChimes chimes;
    chimes.init(48'000.0F);
    chimes.setDensity(1.0F);
    chimes.setSpread(1.0F);
    chimes.setTimbre(0.7F);

    std::array<float, 48'000> left{};
    std::array<float, 48'000> right{};
    galerna::core::AudioBuffer buffer{left, right};

    chimes.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 4.0F);
    }
    for (float sample : right)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 4.0F);
    }
}

TEST_CASE("WindChimes writes an identical mono signal to both channels")
{
    galerna::effects::WindChimes chimes;
    chimes.init(48'000.0F);
    chimes.setDensity(1.0F);
    chimes.setTimbre(0.6F);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    galerna::core::AudioBuffer buffer{left, right};

    chimes.processBlock(buffer);

    REQUIRE(left == right);
}

TEST_CASE("WindChimes timbre changes the generated signal")
{
    galerna::effects::WindChimes chimesDark;
    chimesDark.init(48'000.0F);
    chimesDark.setDensity(1.0F);
    chimesDark.setTimbre(0.02F);

    galerna::effects::WindChimes chimesBright;
    chimesBright.init(48'000.0F);
    chimesBright.setDensity(1.0F);
    chimesBright.setTimbre(0.98F);

    std::array<float, 4'096> leftDark{};
    std::array<float, 4'096> rightDark{};
    std::array<float, 4'096> leftBright{};
    std::array<float, 4'096> rightBright{};
    galerna::core::AudioBuffer bufferDark{leftDark, rightDark};
    galerna::core::AudioBuffer bufferBright{leftBright, rightBright};

    chimesDark.processBlock(bufferDark);
    chimesBright.processBlock(bufferBright);

    REQUIRE(leftDark != leftBright);
}

TEST_CASE("WindChimes active voice count limits how many voices are summed")
{
    galerna::effects::WindChimes chimesFew;
    chimesFew.init(48'000.0F);
    chimesFew.setActiveVoiceCount(1U);
    chimesFew.setDensity(1.0F);

    galerna::effects::WindChimes chimesMany;
    chimesMany.init(48'000.0F);
    chimesMany.setActiveVoiceCount(galerna::effects::WindChimes::voiceCount);
    chimesMany.setDensity(1.0F);

    std::array<float, 4'096> leftFew{};
    std::array<float, 4'096> rightFew{};
    std::array<float, 4'096> leftMany{};
    std::array<float, 4'096> rightMany{};
    galerna::core::AudioBuffer bufferFew{leftFew, rightFew};
    galerna::core::AudioBuffer bufferMany{leftMany, rightMany};

    chimesFew.processBlock(bufferFew);
    chimesMany.processBlock(bufferMany);

    REQUIRE(leftFew != leftMany);
}
