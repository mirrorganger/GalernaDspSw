#include "galerna/core/Xorshift32.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Xorshift32 is deterministic for a given seed")
{
    galerna::core::Xorshift32 first{42U};
    galerna::core::Xorshift32 second{42U};

    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(first.next() == second.next());
    }
}

TEST_CASE("Xorshift32 different seeds diverge")
{
    galerna::core::Xorshift32 first{1U};
    galerna::core::Xorshift32 second{2U};

    bool everDifferent = false;
    for (int i = 0; i < 10; ++i)
    {
        if (first.next() != second.next())
        {
            everDifferent = true;
        }
    }
    REQUIRE(everDifferent);
}

TEST_CASE("Xorshift32 remaps a zero seed to avoid the all-zero fixed point")
{
    galerna::core::Xorshift32 rng{0U};

    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(rng.next() != 0U);
    }
}

TEST_CASE("Xorshift32 nextFloat01 stays within the unit interval")
{
    galerna::core::Xorshift32 rng{7U};

    for (int i = 0; i < 1000; ++i)
    {
        const float value = rng.nextFloat01();
        REQUIRE(value >= 0.0F);
        REQUIRE(value < 1.0F);
    }
}
