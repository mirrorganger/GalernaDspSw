#include "galerna/core/AudioBuffer.hpp"
#include "galerna/core/PentatonicScale.hpp"
#include "galerna/effects/PluckVoice.hpp"
#include "galerna/effects/TwinPluck.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("TwinPluck is silent when no voice has been gated on")
{
    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    galerna::core::AudioBuffer buffer{left, right};
    twinPluck.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(sample == 0.0F);
    }
    for (float sample : right)
    {
        REQUIRE(sample == 0.0F);
    }
}

TEST_CASE("TwinPluck gating one voice on does not gate the others")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 0.5F);
    twinPluck.noteOn(TwinPluck::button1Voice);

    REQUIRE(twinPluck.isVoiceRinging(TwinPluck::button1Voice));
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::button2Voice));
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::droneRootVoice));
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::droneFifthVoice));
}

TEST_CASE("TwinPluck voice stays ringing while held and only releases after noteOff")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.setDecay(0.0F); // shortest possible release, to keep the test fast
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 0.5F);
    twinPluck.noteOn(TwinPluck::button1Voice);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    for (int block = 0; block < 100; ++block)
    {
        galerna::core::AudioBuffer buffer{left, right};
        twinPluck.processBlock(buffer);
    }
    REQUIRE(twinPluck.isVoiceRinging(TwinPluck::button1Voice));

    twinPluck.noteOff(TwinPluck::button1Voice);
    for (int block = 0; block < 100; ++block)
    {
        galerna::core::AudioBuffer buffer{left, right};
        twinPluck.processBlock(buffer);
    }
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::button1Voice));
}

TEST_CASE("TwinPluck drone voices are pre-configured with fixed pitches at init")
{
    using galerna::effects::PluckVoice;
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.noteOn(TwinPluck::droneRootVoice);
    twinPluck.noteOn(TwinPluck::droneFifthVoice);

    REQUIRE(twinPluck.voiceFrequencyHz(TwinPluck::droneRootVoice) == PluckVoice::rootFrequencyHz);
    REQUIRE(
        twinPluck.voiceFrequencyHz(TwinPluck::droneFifthVoice)
        == PluckVoice::rootFrequencyHz
            * galerna::core::PentatonicScale::ratios[TwinPluck::droneFifthScaleDegree]);
    // The two drone pitches must actually differ, or "root and fifth" would be a no-op.
    REQUIRE(
        twinPluck.voiceFrequencyHz(TwinPluck::droneRootVoice)
        != twinPluck.voiceFrequencyHz(TwinPluck::droneFifthVoice));
}

TEST_CASE("TwinPluck drone voices gate independently of the button voices and each other")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.noteOn(TwinPluck::droneRootVoice);

    REQUIRE(twinPluck.isVoiceRinging(TwinPluck::droneRootVoice));
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::droneFifthVoice));
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::button1Voice));
    REQUIRE_FALSE(twinPluck.isVoiceRinging(TwinPluck::button2Voice));
}

TEST_CASE("TwinPluck drone voice pitch wanders away from its exact center while held")
{
    using galerna::effects::PluckVoice;
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.noteOn(TwinPluck::droneRootVoice);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    bool everDifferentFromCenter = false;
    for (int block = 0; block < 4'000; ++block) // ~21s of audio, several wander retargets
    {
        galerna::core::AudioBuffer buffer{left, right};
        twinPluck.processBlock(buffer);
        if (twinPluck.voiceFrequencyHz(TwinPluck::droneRootVoice) != PluckVoice::rootFrequencyHz)
        {
            everDifferentFromCenter = true;
        }
    }
    REQUIRE(everDifferentFromCenter);
}

TEST_CASE("TwinPluck drone voice pitch wander stays close to its center, doesn't run away")
{
    using galerna::effects::PluckVoice;
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.noteOn(TwinPluck::droneRootVoice);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    // Generous +-10% bound: this intentionally doesn't pin the exact (private) wander depth
    // constant, just guards against the wander runaway-diverging or blowing up.
    const float minHz = PluckVoice::rootFrequencyHz * 0.9F;
    const float maxHz = PluckVoice::rootFrequencyHz * 1.1F;
    for (int block = 0; block < 4'000; ++block)
    {
        galerna::core::AudioBuffer buffer{left, right};
        twinPluck.processBlock(buffer);
        const float frequencyHz = twinPluck.voiceFrequencyHz(TwinPluck::droneRootVoice);
        REQUIRE(frequencyHz >= minHz);
        REQUIRE(frequencyHz <= maxHz);
    }
}

TEST_CASE("TwinPluck button voice pitch does not wander")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 0.5F);
    twinPluck.noteOn(TwinPluck::button1Voice);
    const float initialFrequencyHz = twinPluck.voiceFrequencyHz(TwinPluck::button1Voice);

    std::array<float, 256> left{};
    std::array<float, 256> right{};
    for (int block = 0; block < 4'000; ++block)
    {
        galerna::core::AudioBuffer buffer{left, right};
        twinPluck.processBlock(buffer);
        REQUIRE(twinPluck.voiceFrequencyHz(TwinPluck::button1Voice) == initialFrequencyHz);
    }
}

TEST_CASE("TwinPluck retunes a held button voice immediately when the pitch pot moves")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 0.0F);
    twinPluck.noteOn(TwinPluck::button1Voice);
    REQUIRE(twinPluck.voiceFrequencyHz(TwinPluck::button1Voice) == galerna::effects::PluckVoice::rootFrequencyHz);

    // Moving the pitch pot while the button is still held should retune the sounding note right
    // away, not just queue it for the next press.
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 1.0F);
    REQUIRE(twinPluck.voiceFrequencyHz(TwinPluck::button1Voice) != galerna::effects::PluckVoice::rootFrequencyHz);
}

TEST_CASE("TwinPluck writes an identical signal to both channels")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 0.2F);
    twinPluck.noteOn(TwinPluck::button1Voice);
    twinPluck.setVoicePitch(TwinPluck::button2Voice, 0.8F);
    twinPluck.noteOn(TwinPluck::button2Voice);
    twinPluck.noteOn(TwinPluck::droneRootVoice);
    twinPluck.noteOn(TwinPluck::droneFifthVoice);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    galerna::core::AudioBuffer buffer{left, right};
    twinPluck.processBlock(buffer);

    REQUIRE(left == right);
}

TEST_CASE("TwinPluck output stays bounded and finite while all four voices ring")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck twinPluck;
    twinPluck.init(48'000.0F);
    twinPluck.setResonance(1.0F);
    twinPluck.setVoicePitch(TwinPluck::button1Voice, 0.0F);
    twinPluck.noteOn(TwinPluck::button1Voice);
    twinPluck.setVoicePitch(TwinPluck::button2Voice, 1.0F);
    twinPluck.noteOn(TwinPluck::button2Voice);
    twinPluck.noteOn(TwinPluck::droneRootVoice);
    twinPluck.noteOn(TwinPluck::droneFifthVoice);

    std::array<float, 48'000> left{};
    std::array<float, 48'000> right{};
    galerna::core::AudioBuffer buffer{left, right};
    twinPluck.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 8.0F);
    }
}

TEST_CASE("TwinPluck decay controls how long a released voice keeps ringing")
{
    using galerna::effects::TwinPluck;

    galerna::effects::TwinPluck shortDecay;
    shortDecay.init(48'000.0F);
    shortDecay.setDecay(0.0F); // shortest release
    shortDecay.setVoicePitch(TwinPluck::button1Voice, 0.5F);
    shortDecay.noteOn(TwinPluck::button1Voice);
    shortDecay.noteOff(TwinPluck::button1Voice);

    galerna::effects::TwinPluck longDecay;
    longDecay.init(48'000.0F);
    longDecay.setDecay(1.0F); // longest release
    longDecay.setVoicePitch(TwinPluck::button1Voice, 0.5F);
    longDecay.noteOn(TwinPluck::button1Voice);
    longDecay.noteOff(TwinPluck::button1Voice);

    for (int block = 0; block < 48'000 / 256; ++block)
    {
        std::array<float, 256> shortLeft{};
        std::array<float, 256> shortRight{};
        galerna::core::AudioBuffer shortBuffer{shortLeft, shortRight};
        shortDecay.processBlock(shortBuffer);

        std::array<float, 256> longLeft{};
        std::array<float, 256> longRight{};
        galerna::core::AudioBuffer longBuffer{longLeft, longRight};
        longDecay.processBlock(longBuffer);
    }

    REQUIRE_FALSE(shortDecay.isVoiceRinging(TwinPluck::button1Voice));
    REQUIRE(longDecay.isVoiceRinging(TwinPluck::button1Voice));
}
