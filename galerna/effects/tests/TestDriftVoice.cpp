#include "galerna/core/PentatonicScale.hpp"
#include "galerna/effects/DriftVoice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("DriftVoice produces sound as soon as it's initialized, with no gating required")
{
    galerna::effects::DriftVoice voice;
    voice.init(48'000.0F, 1U, 2U);

    bool everNonzero = false;
    for (int sample = 0; sample < 1'000; ++sample)
    {
        if (voice.process() != 0.0F)
        {
            everNonzero = true;
        }
    }
    REQUIRE(everNonzero);
}

TEST_CASE("DriftVoice pitch stays within the pentatonic scale's octave range over time")
{
    using galerna::effects::DriftVoice;

    galerna::effects::DriftVoice voice;
    voice.init(48'000.0F, 1U, 2U);
    voice.setEvolveRate(1.0F); // fastest note-change rate, to exercise several targets
    voice.setDriftDepth(1.0F); // full wobble, so this also bounds the wobble layer

    // Generous headroom around the documented scale/octave range for wobble + glide overshoot,
    // same reasoning as TwinPluck's own wander-bound test.
    const float minHz = DriftVoice::rootFrequencyHz * 0.9F;
    const float maxHz = DriftVoice::rootFrequencyHz
        * galerna::core::PentatonicScale::ratios[galerna::core::PentatonicScale::degreeCount - 1U]
        * std::pow(2.0F, static_cast<float>(DriftVoice::octaveRange - 1U)) * 1.1F;

    for (int block = 0; block < 4'000; ++block)
    {
        voice.updateDrift(64U);
        const float frequencyHz = voice.frequencyHz();
        REQUIRE(frequencyHz >= minHz);
        REQUIRE(frequencyHz <= maxHz);
    }
}

TEST_CASE("DriftVoice setFrozen stops note changes while wobble keeps the pitch moving")
{
    galerna::effects::DriftVoice frozen;
    frozen.init(48'000.0F, 1U, 2U);
    frozen.setEvolveRate(1.0F); // would otherwise change notes quickly
    frozen.setFrozen(true);

    for (int block = 0; block < 200; ++block)
    {
        frozen.updateDrift(64U);
    }
    const float settledHz = frozen.frequencyHz();

    bool everDifferentFromSettled = false;
    for (int block = 0; block < 2'000; ++block)
    {
        frozen.updateDrift(64U);
        if (frozen.frequencyHz() != settledHz)
        {
            everDifferentFromSettled = true;
        }
        // Never strays far from the settled chord -- confirms this is wobble, not a note change.
        REQUIRE(std::abs(frozen.frequencyHz() - settledHz) < settledHz * 0.05F);
    }
    REQUIRE(everDifferentFromSettled);
}

TEST_CASE("DriftVoice reseed forces an immediate note re-target instead of waiting for the timer")
{
    galerna::effects::DriftVoice voice;
    voice.init(48'000.0F, 1U, 2U);
    voice.setEvolveRate(0.0F); // slowest possible -- without reseed(), no change for a long time
    voice.setDriftDepth(0.0F); // isolate note changes from wobble
    voice.updateDrift(64U);
    const float initialHz = voice.frequencyHz();

    voice.reseed();
    bool everChanged = false;
    for (int block = 0; block < 200; ++block) // glide takes a few blocks to become observable
    {
        voice.updateDrift(64U);
        if (voice.frequencyHz() != initialHz)
        {
            everChanged = true;
        }
    }
    REQUIRE(everChanged);
}

TEST_CASE("DriftVoice setDetuneRatio shifts the reported frequency")
{
    galerna::effects::DriftVoice voice;
    voice.init(48'000.0F, 1U, 2U);
    voice.updateDrift(64U);
    const float unisonHz = voice.frequencyHz();

    voice.setDetuneRatio(1.02F);
    REQUIRE(voice.frequencyHz() == unisonHz * 1.02F);
}
