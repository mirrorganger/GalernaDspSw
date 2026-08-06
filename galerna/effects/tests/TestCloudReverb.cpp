#include "galerna/core/AudioBuffer.hpp"
#include "galerna/effects/CloudReverb.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("CloudReverb output equals the dry input exactly at mix zero")
{
    galerna::effects::CloudReverb reverb;
    reverb.init(48'000.0F);
    reverb.setMix(0.0F);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    for (std::size_t sample = 0U; sample < left.size(); ++sample)
    {
        left[sample] = (sample % 5 == 0) ? 1.0F : 0.0F;
        right[sample] = left[sample];
    }
    const std::array<float, 256> expectedLeft = left;
    const std::array<float, 256> expectedRight = right;

    galerna::core::AudioBuffer buffer{left, right};
    reverb.processBlock(buffer);

    REQUIRE(left == expectedLeft);
    REQUIRE(right == expectedRight);
}

TEST_CASE("CloudReverb output stays bounded and finite at full mix")
{
    galerna::effects::CloudReverb reverb;
    reverb.init(48'000.0F);
    reverb.setMix(1.0F);
    reverb.setSize(1.0F);

    std::array<float, 48'000> left{};
    std::array<float, 48'000> right{};
    for (std::size_t sample = 0U; sample < left.size(); ++sample)
    {
        left[sample] = (sample % 200 == 0) ? 1.0F : 0.0F;
        right[sample] = left[sample];
    }

    galerna::core::AudioBuffer buffer{left, right};
    reverb.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 8.0F);
    }
    for (float sample : right)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 8.0F);
    }
}

TEST_CASE("CloudReverb writes an identical wet signal to both channels for a mono input")
{
    // CloudReverb is a single shared mono tank (see its class comment for why -- the original
    // per-channel-tank design measured ~5.8x over the real hardware CPU budget), so it
    // deliberately reads only the left channel as its dry input and mirrors the same wet signal
    // to both outputs, same shape as WindChimes' own mono-duplicated output.
    galerna::effects::CloudReverb reverb;
    reverb.init(48'000.0F);
    reverb.setMix(1.0F);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    for (std::size_t sample = 0U; sample < left.size(); ++sample)
    {
        left[sample] = (sample % 97 == 0) ? 1.0F : 0.0F;
        right[sample] = left[sample];
    }

    galerna::core::AudioBuffer buffer{left, right};
    reverb.processBlock(buffer);

    REQUIRE(left == right);
}

TEST_CASE("CloudReverb size changes the generated signal")
{
    galerna::effects::CloudReverb reverbSmall;
    reverbSmall.init(48'000.0F);
    reverbSmall.setMix(1.0F);
    reverbSmall.setSize(0.0F);

    galerna::effects::CloudReverb reverbLarge;
    reverbLarge.init(48'000.0F);
    reverbLarge.setMix(1.0F);
    reverbLarge.setSize(1.0F);

    std::array<float, 4'096> leftSmall{};
    std::array<float, 4'096> rightSmall{};
    std::array<float, 4'096> leftLarge{};
    std::array<float, 4'096> rightLarge{};
    for (std::size_t sample = 0U; sample < leftSmall.size(); ++sample)
    {
        const float input = (sample % 97 == 0) ? 1.0F : 0.0F;
        leftSmall[sample] = input;
        rightSmall[sample] = input;
        leftLarge[sample] = input;
        rightLarge[sample] = input;
    }

    galerna::core::AudioBuffer bufferSmall{leftSmall, rightSmall};
    galerna::core::AudioBuffer bufferLarge{leftLarge, rightLarge};
    reverbSmall.processBlock(bufferSmall);
    reverbLarge.processBlock(bufferLarge);

    REQUIRE(leftSmall != leftLarge);
}
