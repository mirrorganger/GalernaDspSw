#include "Galerna/App/ThxDeepNoteApp.hpp"
#include "Galerna/Drivers/PotMux4051.hpp"
#include "Tests/Fakes/FakeAdc.hpp"
#include "Tests/Fakes/FakeGpio.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <functional>

namespace
{

struct FakeEffect
{
    static constexpr std::size_t voiceCount{7U};

    void init(float sampleRateHz)
    {
        initSampleRateHz = sampleRateHz;
    }

    void setPitch(float value)
    {
        pitch = value;
    }

    void setPitchShift(float value)
    {
        pitchShift = value;
    }

    void setTimbre(float value)
    {
        timbre = value;
    }

    void setResonance(float value)
    {
        resonance = value;
    }

    void setActiveVoiceCount(std::size_t value)
    {
        activeVoiceCount = value;
    }

    float initSampleRateHz{};
    float pitch{};
    float pitchShift{};
    float timbre{};
    float resonance{};
    std::size_t activeVoiceCount{};
};

struct FakeAudioEngine
{
    bool start()
    {
        started = true;
        return startResult;
    }

    bool started{false};
    bool startResult{true};
};

using App = galerna::app::ThxDeepNoteApp<FakeGpio, FakeAdc, FakeGpio, FakeAudioEngine, FakeEffect>;

struct Fixture
{
    FakeAdc adc;
    FakeGpio muxSel0;
    FakeGpio muxSel1;
    FakeGpio muxSel2;
    std::array<std::reference_wrapper<FakeGpio>, 3> muxSelectLines{
        std::ref(muxSel0), std::ref(muxSel1), std::ref(muxSel2)};
    galerna::drivers::PotMux4051<FakeAdc, FakeGpio> potMux{adc, muxSelectLines, std::uint8_t{0}};

    FakeGpio led0;
    FakeGpio led1;
    FakeGpio led2;
    std::array<std::reference_wrapper<FakeGpio>, 3> leds{std::ref(led0), std::ref(led1), std::ref(led2)};

    App::PotMuxChannels potMuxChannels{
        .pitch = 1U, .pitchShift = 3U, .timbre = 5U, .resonance = 7U, .voiceCount = 6U};

    FakeAudioEngine audioEngine;
    FakeEffect effect;
};

} // namespace

TEST_CASE("ThxDeepNoteApp::init inits the effect at the given sample rate and starts the audio engine")
{
    Fixture fixture;
    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    REQUIRE(app.init() == true);

    REQUIRE(fixture.effect.initSampleRateHz == 32'000.0F);
    REQUIRE(fixture.audioEngine.started == true);
}

TEST_CASE("ThxDeepNoteApp::init returns false when the audio engine fails to start")
{
    Fixture fixture;
    fixture.audioEngine.startResult = false;
    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    REQUIRE(app.init() == false);
}

TEST_CASE("ThxDeepNoteApp::tick maps the pot reading onto every THX control")
{
    Fixture fixture;
    fixture.adc.value = 4'095U; // == App::potMaxValue -> normalized 1.0
    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    app.tick();

    REQUIRE(fixture.effect.pitch == 1.0F);
    REQUIRE(fixture.effect.pitchShift == 1.0F);
    REQUIRE(fixture.effect.timbre == 1.0F);
    REQUIRE(fixture.effect.resonance == 1.0F);
}

TEST_CASE("ThxDeepNoteApp::tick rounds the voice-count pot and returns the active voice count")
{
    Fixture fixture;
    fixture.adc.value = 4'095U; // normalized 1.0 -> round(1.0 * FakeEffect::voiceCount) == 7
    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    const auto activeVoiceCount = app.tick();

    REQUIRE(activeVoiceCount == 7U);
    REQUIRE(fixture.effect.activeVoiceCount == 7U);
}

TEST_CASE("ThxDeepNoteApp::tick mirrors the active voice count on the status LEDs in binary")
{
    Fixture fixture;
    fixture.adc.value = 4'095U; // -> active voice count 7 == 0b111
    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    app.tick();

    REQUIRE(fixture.led0.state == true);
    REQUIRE(fixture.led1.state == true);
    REQUIRE(fixture.led2.state == true);
}

TEST_CASE("ThxDeepNoteApp::tick turns the status LEDs off when the voice-count pot is at minimum")
{
    Fixture fixture;
    fixture.adc.value = 0U; // -> active voice count 0

    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    const auto activeVoiceCount = app.tick();

    REQUIRE(activeVoiceCount == 0U);
    REQUIRE(fixture.led0.state == false);
    REQUIRE(fixture.led1.state == false);
    REQUIRE(fixture.led2.state == false);
}

TEST_CASE("ThxDeepNoteApp::tick selects the voice-count pot mux channel last")
{
    Fixture fixture;
    App app{fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.audioEngine, fixture.effect, 32'000.0F};

    app.tick();

    // voiceCount channel is 6 == binary 110, read last -> select lines end up 0,1,1.
    REQUIRE(fixture.muxSel0.state == false);
    REQUIRE(fixture.muxSel1.state == true);
    REQUIRE(fixture.muxSel2.state == true);
}
