#include "galerna/effects/WindChimeVoice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("WindChimeVoice strikes immediately on the very first process() call")
{
    // No update() has run yet at init() time, so there's no real density to schedule an initial
    // wait from -- the voice takes its first strike right away instead (see WindChimeVoice::init
    // doc comment) and amplitude only decays after that.
    galerna::effects::WindChimeVoice voice;
    voice.init(48'000.0F, 1U);

    REQUIRE(voice.process() != 0.0F);
}

TEST_CASE("WindChimeVoice falls silent again after its first strike rings out, at zero density")
{
    galerna::effects::WindChimeVoice voice;
    voice.init(48'000.0F, 1U);
    voice.update(0.0F, 0.5F, 1.4F); // density 0 -> longest possible wait before the *next* strike

    // ringDurationS (1.4s) worth of samples, plus margin, is enough for the immediate first
    // strike to decay below silenceThreshold and hand control back to the (very long, at
    // density 0) wait for the next one.
    for (int sample = 0; sample < 48'000 * 2; ++sample)
    {
        voice.process();
    }

    for (int sample = 0; sample < 100; ++sample)
    {
        const float value = voice.process();
        REQUIRE(std::isfinite(value));
        REQUIRE(value == 0.0F);
    }
}

TEST_CASE("WindChimeVoice decay controls how long a struck note keeps ringing")
{
    // Same seed and density 0 on both -> same single strike at t=0 (immediate first strike, see
    // init()) with the next one at least 2s away, isolating decay's effect on that one strike.
    galerna::effects::WindChimeVoice shortDecay;
    shortDecay.init(48'000.0F, 5U);
    shortDecay.update(0.0F, 1.0F, 0.1F);

    galerna::effects::WindChimeVoice longDecay;
    longDecay.init(48'000.0F, 5U);
    longDecay.update(0.0F, 1.0F, 5.0F);

    for (int sample = 0; sample < 48'000; ++sample)
    {
        shortDecay.process();
        longDecay.process();
    }

    // One second in: the 0.1s-decay voice's only strike so far has long since rung out, while
    // the 5.0s-decay voice (same strike) is still well within its ring.
    REQUIRE(shortDecay.process() == 0.0F);
    REQUIRE(longDecay.process() != 0.0F);
}

TEST_CASE("WindChimeVoice eventually strikes and rings out at maximum density")
{
    galerna::effects::WindChimeVoice voice;
    voice.init(48'000.0F, 2U);
    voice.update(1.0F, 1.0F, 1.4F); // density 1 -> shortest possible wait before striking

    bool everNonZero = false;
    for (int sample = 0; sample < 48'000 * 2; ++sample)
    {
        const float value = voice.process();
        REQUIRE(std::isfinite(value));
        if (value != 0.0F)
        {
            everNonZero = true;
        }
    }
    REQUIRE(everNonZero);
}

TEST_CASE("WindChimeVoice frequency stays within its documented chime range")
{
    galerna::effects::WindChimeVoice voice;
    voice.init(48'000.0F, 3U);
    voice.update(1.0F, 1.0F, 1.4F); // spread 1 -> widest possible octave range, most likely to escape bounds

    for (int sample = 0; sample < 48'000 * 4; ++sample)
    {
        voice.process();
        REQUIRE(voice.frequencyHz() >= galerna::effects::WindChimeVoice::lowestFrequencyHz);
        REQUIRE(voice.frequencyHz() <= galerna::effects::WindChimeVoice::highestFrequencyHz);
    }
}

TEST_CASE("WindChimeVoice with the same seed and controls repeats the same schedule")
{
    galerna::effects::WindChimeVoice first;
    first.init(48'000.0F, 42U);
    first.update(1.0F, 1.0F, 1.4F);

    galerna::effects::WindChimeVoice second;
    second.init(48'000.0F, 42U);
    second.update(1.0F, 1.0F, 1.4F);

    for (int sample = 0; sample < 48'000; ++sample)
    {
        REQUIRE(first.process() == second.process());
    }
}

TEST_CASE("WindChimeVoice with different seeds diverges")
{
    galerna::effects::WindChimeVoice first;
    first.init(48'000.0F, 1U);
    first.update(1.0F, 1.0F, 1.4F);

    galerna::effects::WindChimeVoice second;
    second.init(48'000.0F, 99U);
    second.update(1.0F, 1.0F, 1.4F);

    bool everDifferent = false;
    for (int sample = 0; sample < 48'000; ++sample)
    {
        if (first.process() != second.process())
        {
            everDifferent = true;
        }
    }
    REQUIRE(everDifferent);
}
