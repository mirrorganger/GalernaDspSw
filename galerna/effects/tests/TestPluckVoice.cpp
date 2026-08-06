#include "galerna/core/PentatonicScale.hpp"
#include "galerna/effects/PluckVoice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("PluckVoice is silent until noteOn()'d")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);

    for (int sample = 0; sample < 1'000; ++sample)
    {
        REQUIRE(voice.process() == 0.0F);
    }
    REQUIRE_FALSE(voice.isRinging());
}

TEST_CASE("PluckVoice sustains at full amplitude while held, regardless of decay setting")
{
    // Hold for far longer than any release duration this instrument uses -- confirm it's still
    // ringing (i.e. it isn't secretly decaying while held) rather than checking amplitude
    // directly, which would require exposing internal state.
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.setPendingFrequency(0.5F);
    voice.noteOn();

    REQUIRE(voice.isRinging());
    REQUIRE(voice.process() != 0.0F);

    for (int sample = 0; sample < 48'000 * 10; ++sample)
    {
        voice.process();
    }

    REQUIRE(voice.isRinging());
}

TEST_CASE("PluckVoice only starts releasing after noteOff() and eventually falls silent")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.setPendingFrequency(0.5F);
    voice.noteOn();
    voice.noteOff(0.1F);

    REQUIRE(voice.isRinging());

    for (int sample = 0; sample < 48'000; ++sample)
    {
        voice.process();
    }

    REQUIRE_FALSE(voice.isRinging());
    REQUIRE(voice.process() == 0.0F);
}

TEST_CASE("PluckVoice noteOff() with nothing held is a no-op")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.noteOff(0.1F);

    REQUIRE_FALSE(voice.isRinging());
    REQUIRE(voice.process() == 0.0F);
}

TEST_CASE("PluckVoice can be re-noteOn()'d mid-release")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.setPendingFrequency(0.5F);
    voice.noteOn();
    voice.noteOff(2.0F);

    for (int sample = 0; sample < 100; ++sample)
    {
        voice.process();
    }

    // Re-pressed before the release finished -- should cleanly restart at full amplitude rather
    // than staying in (or getting stuck in) the releasing state.
    voice.noteOn();
    REQUIRE(voice.isRinging());

    for (int sample = 0; sample < 48'000 * 10; ++sample)
    {
        voice.process();
    }
    REQUIRE(voice.isRinging());
}

TEST_CASE("PluckVoice setPendingFrequency retunes a currently-held note immediately")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.setPendingFrequency(0.0F);
    voice.noteOn();
    REQUIRE(voice.frequencyHz() == galerna::effects::PluckVoice::rootFrequencyHz);

    // Turning the pitch knob while held retunes the note right away -- so a "keyboard" feel
    // where moving the pot changes what you hear immediately, not just on the next press.
    voice.setPendingFrequency(1.0F);
    REQUIRE(voice.frequencyHz() != galerna::effects::PluckVoice::rootFrequencyHz);
}

TEST_CASE("PluckVoice setPendingFrequency does not retune a silent (idle) voice until noteOn()")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);

    // Nothing held yet -- this only sets what the *next* noteOn() will sound like.
    voice.setPendingFrequency(0.0F);
    REQUIRE_FALSE(voice.isRinging());

    voice.setPendingFrequency(1.0F);
    voice.noteOn();
    const float expectedTopHz = galerna::effects::PluckVoice::rootFrequencyHz
        * galerna::core::PentatonicScale::ratios[galerna::core::PentatonicScale::degreeCount - 1U]
        * 4.0F;
    REQUIRE(voice.frequencyHz() == expectedTopHz);
}

TEST_CASE("PluckVoice quantizes pitch to the documented pentatonic scale")
{
    using galerna::effects::PluckVoice;

    galerna::effects::PluckVoice rootVoice;
    rootVoice.init(48'000.0F);
    rootVoice.setPendingFrequency(0.0F);
    rootVoice.noteOn();
    REQUIRE(rootVoice.frequencyHz() == PluckVoice::rootFrequencyHz);

    // Top of the pot's travel should land on the highest octave's highest scale degree:
    // octave index = octaveRange - 1 = 2 -> 2^2 = 4x,
    // degree index = core::PentatonicScale::degreeCount - 1.
    galerna::effects::PluckVoice topVoice;
    topVoice.init(48'000.0F);
    topVoice.setPendingFrequency(1.0F);
    topVoice.noteOn();
    const float expectedTopHz = PluckVoice::rootFrequencyHz
        * galerna::core::PentatonicScale::ratios[galerna::core::PentatonicScale::degreeCount - 1U] * 4.0F;
    REQUIRE(topVoice.frequencyHz() == expectedTopHz);

    // Midway up the pot's travel should land on a different note than either extreme.
    galerna::effects::PluckVoice midVoice;
    midVoice.init(48'000.0F);
    midVoice.setPendingFrequency(0.5F);
    midVoice.noteOn();
    REQUIRE(midVoice.frequencyHz() != rootVoice.frequencyHz());
    REQUIRE(midVoice.frequencyHz() != topVoice.frequencyHz());
}

TEST_CASE("PluckVoice modulateFrequencyHz updates the live oscillator without touching the pending frequency")
{
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.setPendingFrequency(0.0F);
    voice.noteOn();
    REQUIRE(voice.frequencyHz() == galerna::effects::PluckVoice::rootFrequencyHz);

    voice.modulateFrequencyHz(440.0F);
    REQUIRE(voice.frequencyHz() == 440.0F);

    // Unlike setPendingFrequency()/setPendingFrequencyHz(), modulateFrequencyHz() never updates
    // the pending frequency (still root, from setPendingFrequency(0.0F) above) -- so the next
    // noteOn() reverts to it rather than staying at the modulated value.
    voice.noteOff(0.01F);
    for (int sample = 0; sample < 48'000; ++sample)
    {
        voice.process();
    }
    voice.noteOn();
    REQUIRE(voice.frequencyHz() == galerna::effects::PluckVoice::rootFrequencyHz);
}

TEST_CASE("PluckVoice setPendingFrequencyHz sets an exact frequency, bypassing quantization")
{
    // Used by TwinPluck's fixed-pitch drone voices, which aren't pot-controlled and so shouldn't
    // be snapped onto the core::PentatonicScale::degreeCount*octaveRange grid
    // setPendingFrequency() quantizes to.
    galerna::effects::PluckVoice voice;
    voice.init(48'000.0F);
    voice.setPendingFrequencyHz(440.0F);
    voice.noteOn();

    REQUIRE(voice.frequencyHz() == 440.0F);
}
