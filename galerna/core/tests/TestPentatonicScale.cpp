#include "galerna/core/PentatonicScale.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

TEST_CASE("PentatonicScale ratios start at unison and are monotonically increasing")
{
    REQUIRE(galerna::core::PentatonicScale::ratios[0] == 1.0F);
    for (std::size_t degree = 1U; degree < galerna::core::PentatonicScale::degreeCount; ++degree)
    {
        REQUIRE(galerna::core::PentatonicScale::ratios[degree] > galerna::core::PentatonicScale::ratios[degree - 1U]);
    }
}

TEST_CASE("PentatonicScale ratios all stay within one octave")
{
    for (float ratio : galerna::core::PentatonicScale::ratios)
    {
        REQUIRE(ratio >= 1.0F);
        REQUIRE(ratio < 2.0F);
    }
}

TEST_CASE("PentatonicScale degreeCount matches the ratios array size")
{
    REQUIRE(galerna::core::PentatonicScale::ratios.size() == galerna::core::PentatonicScale::degreeCount);
}
