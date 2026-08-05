#include "galerna/core/BinaryLedDisplay.hpp"
#include "fakes/FakeGpio.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <functional>

TEST_CASE("displayBinary lights LEDs matching each bit, LSB first")
{
    FakeGpio led0;
    FakeGpio led1;
    FakeGpio led2;
    std::array<std::reference_wrapper<FakeGpio>, 3> leds{std::ref(led0), std::ref(led1), std::ref(led2)};

    galerna::core::displayBinary(leds, 5U); // 0b101

    REQUIRE(led0.get() == true);
    REQUIRE(led1.get() == false);
    REQUIRE(led2.get() == true);
}

TEST_CASE("displayBinary turns all LEDs off for zero")
{
    FakeGpio led0;
    FakeGpio led1;
    FakeGpio led2;
    led0.set(true);
    led1.set(true);
    led2.set(true);
    std::array<std::reference_wrapper<FakeGpio>, 3> leds{std::ref(led0), std::ref(led1), std::ref(led2)};

    galerna::core::displayBinary(leds, 0U);

    REQUIRE(led0.get() == false);
    REQUIRE(led1.get() == false);
    REQUIRE(led2.get() == false);
}

TEST_CASE("displayBinary truncates values that don't fit in the LED count")
{
    FakeGpio led0;
    FakeGpio led1;
    std::array<std::reference_wrapper<FakeGpio>, 2> leds{std::ref(led0), std::ref(led1)};

    galerna::core::displayBinary(leds, 6U); // 0b110 -> only the low 2 bits (0b10) are shown

    REQUIRE(led0.get() == false);
    REQUIRE(led1.get() == true);
}
