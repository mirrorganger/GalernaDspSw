#include "galerna/core/Range.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("Range::linear maps 0 and 1 to min and max")
{
    constexpr galerna::core::Range range{100.0F, 200.0F};
    REQUIRE(range.linear(0.0F) == 100.0F);
    REQUIRE(range.linear(1.0F) == 200.0F);
    REQUIRE(range.linear(0.5F) == 150.0F);
}

TEST_CASE("Range::exponential maps 0 and 1 to min and max")
{
    constexpr galerna::core::Range range{150.0F, 5'000.0F};
    REQUIRE(range.exponential(0.0F) == Catch::Approx(150.0F));
    REQUIRE(range.exponential(1.0F) == Catch::Approx(5'000.0F));
}

TEST_CASE("Range::exponential is monotonically increasing with normalized")
{
    constexpr galerna::core::Range range{150.0F, 5'000.0F};
    REQUIRE(range.exponential(0.25F) < range.exponential(0.5F));
    REQUIRE(range.exponential(0.5F) < range.exponential(0.75F));
}

TEST_CASE("Range::exponential(1 - normalized) matches the max*(min/max)^normalized inverted shape")
{
    constexpr galerna::core::Range range{6.0F, 45.0F};
    const float normalized = 0.3F;
    const float inverted = range.max * std::pow(range.min / range.max, normalized);
    REQUIRE(range.exponential(1.0F - normalized) == Catch::Approx(inverted));
}

TEST_CASE("Range::clamp bounds a value into [min, max]")
{
    constexpr galerna::core::Range range{0.05F, 6.0F};
    REQUIRE(range.clamp(-1.0F) == 0.05F);
    REQUIRE(range.clamp(100.0F) == 6.0F);
    REQUIRE(range.clamp(1.0F) == 1.0F);
}
