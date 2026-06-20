#include "Galerna/Drivers/Wm8731Codec.hpp"
#include "Tests/Fakes/FakeI2c.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("WM8731 reset writes reset register")
{
    FakeI2c i2c;
    galerna::drivers::Wm8731Codec codec{i2c};

    REQUIRE(codec.reset());

    REQUIRE(i2c.writes.size() == 1);
    REQUIRE(i2c.writes[0].address == 0x1A);
    REQUIRE(i2c.writes[0].data.size() == 2);
    REQUIRE(i2c.writes[0].data[0] == 0x1E);
    REQUIRE(i2c.writes[0].data[1] == 0x00);
}
