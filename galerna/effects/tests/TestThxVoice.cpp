#include "galerna/effects/ThxVoice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("ThxVoice frequency glide stays within the requested bounds")
{
    galerna::effects::ThxVoice voice;
    voice.init(48'000.0F, 500.0F, 10.0F);

    for (int step = 0; step <= 10; ++step)
    {
        const float pitch = static_cast<float>(step) / 10.0F;
        voice.update(pitch, 0.0F);
        REQUIRE(voice.frequencyHz() >= galerna::effects::ThxVoice::lowestFrequencyHz);
        REQUIRE(voice.frequencyHz() <= 500.0F);
    }
}

TEST_CASE("ThxVoice glide bounds shift by pitchShiftHz")
{
    galerna::effects::ThxVoice voice;
    voice.init(48'000.0F, 500.0F, 10.0F);

    voice.update(0.0F, 200.0F);
    REQUIRE(voice.frequencyHz() == galerna::effects::ThxVoice::lowestFrequencyHz + 200.0F);

    voice.update(1.0F, 200.0F);
    REQUIRE(voice.frequencyHz() == 500.0F + 200.0F);
}

TEST_CASE("ThxVoice LFO amplitude grows with pitch and is zero at pitch 0")
{
    galerna::effects::ThxVoice voice;
    voice.init(48'000.0F, 500.0F, 10.0F);

    voice.update(0.0F, 0.0F);
    REQUIRE(voice.lfoAmplitude() == 0.0F);

    float previousAmplitude = voice.lfoAmplitude();
    for (int step = 1; step <= 10; ++step)
    {
        const float pitch = static_cast<float>(step) / 10.0F;
        voice.update(pitch, 0.0F);
        REQUIRE(voice.lfoAmplitude() >= previousAmplitude);
        previousAmplitude = voice.lfoAmplitude();
    }
    REQUIRE(previousAmplitude > 0.0F);
}

TEST_CASE("ThxVoice process output stays finite across a pitch sweep")
{
    galerna::effects::ThxVoice voice;
    voice.init(48'000.0F, 500.0F, 10.0F);

    for (int step = 0; step <= 10; ++step)
    {
        const float pitch = static_cast<float>(step) / 10.0F;
        voice.update(pitch, 100.0F);
        for (int sample = 0; sample < 100; ++sample)
        {
            REQUIRE(std::isfinite(voice.process()));
        }
    }
}
