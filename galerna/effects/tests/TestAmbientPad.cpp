#include "galerna/core/AudioBuffer.hpp"
#include "galerna/effects/AmbientPad.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("AmbientPad is silent with zero active voices")
{
    galerna::effects::AmbientPad pad;
    pad.init(48'000.0F);
    pad.setActiveVoiceCount(0U);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    left.fill(1.0F);
    right.fill(1.0F);
    galerna::core::AudioBuffer buffer{left, right};

    pad.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(sample == 0.0F);
    }
    for (float sample : right)
    {
        REQUIRE(sample == 0.0F);
    }
}

TEST_CASE("AmbientPad produces sound with at least one active voice, no triggering required")
{
    galerna::effects::AmbientPad pad;
    pad.init(48'000.0F);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    galerna::core::AudioBuffer buffer{left, right};

    pad.processBlock(buffer);

    bool everNonzero = false;
    for (float sample : left)
    {
        if (sample != 0.0F)
        {
            everNonzero = true;
        }
    }
    REQUIRE(everNonzero);
}

TEST_CASE("AmbientPad writes an identical mono signal to both channels")
{
    galerna::effects::AmbientPad pad;
    pad.init(48'000.0F);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    galerna::core::AudioBuffer buffer{left, right};

    pad.processBlock(buffer);

    REQUIRE(left == right);
}

TEST_CASE("AmbientPad output stays bounded and finite at full voice count and resonance")
{
    galerna::effects::AmbientPad pad;
    pad.init(48'000.0F);
    pad.setResonance(1.0F);
    pad.setDetune(1.0F);
    pad.setDriftDepth(1.0F);

    std::array<float, 48'000> left{};
    std::array<float, 48'000> right{};
    galerna::core::AudioBuffer buffer{left, right};

    pad.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 8.0F);
    }
}

TEST_CASE("AmbientPad timbre changes the generated signal")
{
    galerna::effects::AmbientPad padDark;
    padDark.init(48'000.0F);
    padDark.setTimbre(0.02F);

    galerna::effects::AmbientPad padBright;
    padBright.init(48'000.0F);
    padBright.setTimbre(0.98F);

    std::array<float, 4'096> leftDark{};
    std::array<float, 4'096> rightDark{};
    std::array<float, 4'096> leftBright{};
    std::array<float, 4'096> rightBright{};
    galerna::core::AudioBuffer bufferDark{leftDark, rightDark};
    galerna::core::AudioBuffer bufferBright{leftBright, rightBright};

    padDark.processBlock(bufferDark);
    padBright.processBlock(bufferBright);

    REQUIRE(leftDark != leftBright);
}

TEST_CASE("AmbientPad detune changes the generated signal")
{
    galerna::effects::AmbientPad padUnison;
    padUnison.init(48'000.0F);
    padUnison.setDetune(0.0F);

    galerna::effects::AmbientPad padDetuned;
    padDetuned.init(48'000.0F);
    padDetuned.setDetune(1.0F);

    std::array<float, 4'096> leftUnison{};
    std::array<float, 4'096> rightUnison{};
    std::array<float, 4'096> leftDetuned{};
    std::array<float, 4'096> rightDetuned{};
    galerna::core::AudioBuffer bufferUnison{leftUnison, rightUnison};
    galerna::core::AudioBuffer bufferDetuned{leftDetuned, rightDetuned};

    padUnison.processBlock(bufferUnison);
    padDetuned.processBlock(bufferDetuned);

    REQUIRE(leftUnison != leftDetuned);
}

TEST_CASE("AmbientPad active voice count limits how many voices are summed")
{
    galerna::effects::AmbientPad padFew;
    padFew.init(48'000.0F);
    padFew.setActiveVoiceCount(1U);
    padFew.setDetune(1.0F);

    galerna::effects::AmbientPad padMany;
    padMany.init(48'000.0F);
    padMany.setActiveVoiceCount(galerna::effects::AmbientPad::voiceCount);
    padMany.setDetune(1.0F);

    std::array<float, 4'096> leftFew{};
    std::array<float, 4'096> rightFew{};
    std::array<float, 4'096> leftMany{};
    std::array<float, 4'096> rightMany{};
    galerna::core::AudioBuffer bufferFew{leftFew, rightFew};
    galerna::core::AudioBuffer bufferMany{leftMany, rightMany};

    padFew.processBlock(bufferFew);
    padMany.processBlock(bufferMany);

    REQUIRE(leftFew != leftMany);
}

TEST_CASE("AmbientPad setFrozen holds each voice's frequency still across many blocks")
{
    galerna::effects::AmbientPad pad;
    pad.init(48'000.0F);
    pad.setEvolveRate(1.0F); // fastest -- would otherwise definitely change notes below
    pad.setDriftDepth(0.0F); // isolate: no wobble either, so frozen frequency should be exact
    pad.setFrozen(true);

    std::array<float, 64> left{};
    std::array<float, 64> right{};
    galerna::core::AudioBuffer buffer{left, right};
    pad.processBlock(buffer);
    const float voice0Hz = pad.voiceFrequencyHz(0U);

    for (int block = 0; block < 500; ++block)
    {
        pad.processBlock(buffer);
        REQUIRE(pad.voiceFrequencyHz(0U) == voice0Hz);
    }
}

TEST_CASE("AmbientPad reseed forces an immediate note re-target even at the slowest evolve rate")
{
    galerna::effects::AmbientPad pad;
    pad.init(48'000.0F);
    pad.setEvolveRate(0.0F); // slowest -- without reseed(), no change for a long time
    pad.setDriftDepth(0.0F); // isolate note changes from wobble

    std::array<float, 64> left{};
    std::array<float, 64> right{};
    galerna::core::AudioBuffer buffer{left, right};
    pad.processBlock(buffer);
    const float initialHz = pad.voiceFrequencyHz(0U);

    pad.reseed();
    bool everChanged = false;
    for (int block = 0; block < 200; ++block) // glide takes a few blocks to become observable
    {
        pad.processBlock(buffer);
        if (pad.voiceFrequencyHz(0U) != initialHz)
        {
            everChanged = true;
        }
    }
    REQUIRE(everChanged);
}
