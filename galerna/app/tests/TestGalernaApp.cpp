#include "galerna/app/GalernaApp.hpp"
#include "galerna/drivers/PotMux4051.hpp"
#include "fakes/FakeAdc.hpp"
#include "fakes/FakeGpio.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <functional>

namespace
{

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

    std::array<std::uint8_t, 3> potMuxChannels{0U, 1U, 2U};

    // Active-low buttons, pulled up: true == released. Default released so tests aren't
    // accidentally paused.
    FakeGpio btn1{true};
    FakeGpio btn2{true};
    std::array<std::reference_wrapper<FakeGpio>, 2> buttons{std::ref(btn1), std::ref(btn2)};

    // Switches directly drive the pin high/low; default true == enabled so tests aren't
    // accidentally disabled.
    FakeGpio sw1{true};
    FakeGpio sw2{true};
    std::array<std::reference_wrapper<FakeGpio>, 2> switches{std::ref(sw1), std::ref(sw2)};
};

} // namespace

TEST_CASE("GalernaApp turns all LEDs off on init")
{
    Fixture fixture;
    fixture.led0.state = true;
    fixture.led2.state = true;
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};

    app.init();

    REQUIRE(fixture.led0.state == false);
    REQUIRE(fixture.led1.state == false);
    REQUIRE(fixture.led2.state == false);
}

TEST_CASE("GalernaApp does not toggle an LED before its blink half-period elapses")
{
    Fixture fixture;
    fixture.adc.value = 0U; // minimum pot value -> slowest (longest) blink period
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();

    app.tick();

    REQUIRE(fixture.led0.state == false);
}

TEST_CASE("GalernaApp toggles an LED once its blink half-period elapses")
{
    Fixture fixture;
    fixture.adc.value = 4'095U; // maximum pot value -> fastest (shortest) blink period
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();

    // Half-period at max frequency (8 Hz) is 62.5 ms; a handful of 20 ms ticks must cross it.
    for (int i = 0; i < 5; ++i)
    {
        app.tick();
    }

    REQUIRE(fixture.led0.state == true);
}

TEST_CASE("GalernaApp reads each LED's configured pot mux channel")
{
    Fixture fixture;
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();

    app.tick();

    // Last LED (index 2) reads mux channel 2 == binary 010, so select lines end up 0,1,0.
    REQUIRE(fixture.muxSel0.state == false);
    REQUIRE(fixture.muxSel1.state == true);
    REQUIRE(fixture.muxSel2.state == false);
}

TEST_CASE("GalernaApp freezes LED state while any button is held")
{
    Fixture fixture;
    fixture.adc.value = 4'095U; // fastest blink period, would otherwise toggle quickly
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();
    fixture.btn1.state = false; // pressed (active-low)

    for (int i = 0; i < 10; ++i)
    {
        app.tick();
    }

    REQUIRE(fixture.led0.state == false);
    REQUIRE(fixture.led1.state == false);
    REQUIRE(fixture.led2.state == false);
}

TEST_CASE("GalernaApp resumes blinking once the button is released")
{
    Fixture fixture;
    fixture.adc.value = 4'095U;
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();
    fixture.btn2.state = false; // pressed (active-low)

    for (int i = 0; i < 10; ++i)
    {
        app.tick();
    }
    REQUIRE(fixture.led0.state == false);

    fixture.btn2.state = true; // released
    for (int i = 0; i < 5; ++i)
    {
        app.tick();
    }

    REQUIRE(fixture.led0.state == true);
}

TEST_CASE("GalernaApp forces LED0 off while SW1 disables it")
{
    Fixture fixture;
    fixture.adc.value = 4'095U;
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();
    fixture.sw1.state = false; // disabled

    for (int i = 0; i < 10; ++i)
    {
        app.tick();
    }

    REQUIRE(fixture.led0.state == false);
}

TEST_CASE("GalernaApp forces LED1 off while SW2 disables it, LED2 unaffected")
{
    Fixture fixture;
    fixture.adc.value = 4'095U;
    galerna::app::GalernaApp app{
        fixture.leds, fixture.potMux, fixture.potMuxChannels, fixture.buttons, fixture.switches, 20U};
    app.init();
    fixture.sw1.state = false;
    fixture.sw2.state = false;

    // Same tick count as the "toggles once its blink half-period elapses" case above,
    // so LED2 (unaffected by either switch) is expected to have toggled on by now.
    for (int i = 0; i < 5; ++i)
    {
        app.tick();
    }

    REQUIRE(fixture.led0.state == false);
    REQUIRE(fixture.led1.state == false);
    REQUIRE(fixture.led2.state == true); // LED2 has no switch, keeps blinking
}
