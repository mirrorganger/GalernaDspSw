#include "Galerna/Core/AudioBuffer.hpp"
#include "Galerna/Effects/ThxDeepNote.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("ThxDeepNote is silent with zero active voices")
{
    galerna::effects::ThxDeepNote thx;
    thx.init(48'000.0F);
    thx.setActiveVoiceCount(0U);

    std::array<float, 32> left{};
    std::array<float, 32> right{};
    left.fill(1.0F);
    right.fill(1.0F);
    galerna::core::AudioBuffer buffer{left, right};

    thx.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(sample == 0.0F);
    }
    for (float sample : right)
    {
        REQUIRE(sample == 0.0F);
    }
}

TEST_CASE("ThxDeepNote output stays bounded and finite at full voice count")
{
    galerna::effects::ThxDeepNote thx;
    thx.init(48'000.0F);
    thx.setPitch(0.5F);
    thx.setPitchShift(0.3F);
    thx.setTimbre(0.7F);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    galerna::core::AudioBuffer buffer{left, right};

    thx.processBlock(buffer);

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

TEST_CASE("ThxDeepNote writes an identical mono signal to both channels")
{
    galerna::effects::ThxDeepNote thx;
    thx.init(48'000.0F);
    thx.setPitch(0.4F);
    thx.setTimbre(0.6F);

    std::array<float, 128> left{};
    std::array<float, 128> right{};
    galerna::core::AudioBuffer buffer{left, right};

    thx.processBlock(buffer);

    REQUIRE(left == right);
}

TEST_CASE("ThxDeepNote pitch changes the generated signal")
{
    galerna::effects::ThxDeepNote thxScattered;
    thxScattered.init(48'000.0F);
    thxScattered.setPitch(0.0F);

    galerna::effects::ThxDeepNote thxConverged;
    thxConverged.init(48'000.0F);
    thxConverged.setPitch(1.0F);

    std::array<float, 64> leftScattered{};
    std::array<float, 64> rightScattered{};
    std::array<float, 64> leftConverged{};
    std::array<float, 64> rightConverged{};
    galerna::core::AudioBuffer bufferScattered{leftScattered, rightScattered};
    galerna::core::AudioBuffer bufferConverged{leftConverged, rightConverged};

    thxScattered.processBlock(bufferScattered);
    thxConverged.processBlock(bufferConverged);

    REQUIRE(leftScattered != leftConverged);
}

TEST_CASE("ThxDeepNote timbre changes the generated signal")
{
    galerna::effects::ThxDeepNote thxDark;
    thxDark.init(48'000.0F);
    thxDark.setPitch(0.5F);
    thxDark.setTimbre(0.02F);

    galerna::effects::ThxDeepNote thxBright;
    thxBright.init(48'000.0F);
    thxBright.setPitch(0.5F);
    thxBright.setTimbre(0.98F);

    std::array<float, 64> leftDark{};
    std::array<float, 64> rightDark{};
    std::array<float, 64> leftBright{};
    std::array<float, 64> rightBright{};
    galerna::core::AudioBuffer bufferDark{leftDark, rightDark};
    galerna::core::AudioBuffer bufferBright{leftBright, rightBright};

    thxDark.processBlock(bufferDark);
    thxBright.processBlock(bufferBright);

    REQUIRE(leftDark != leftBright);
}
