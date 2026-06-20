#include "Galerna/Drivers/PotMux4051.hpp"
#include "Tests/Fakes/FakeAdc.hpp"
#include "Tests/Fakes/FakeGpio.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PotMux4051 selects requested mux channel")
{
    FakeAdc adc;
    FakeGpio s0;
    FakeGpio s1;
    FakeGpio s2;

    galerna::drivers::PotMux4051 mux{adc, s0, s1, s2, 5};

    const auto value = mux.read(6);

    REQUIRE(value == 1234);
    REQUIRE(adc.lastChannel == 5);

    REQUIRE(s0.state == false);
    REQUIRE(s1.state == true);
    REQUIRE(s2.state == true);
}
