#include "Galerna/Core/TestTone.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

TEST_CASE("TestTone fills interleaved stereo sine samples")
{
    galerna::core::TestTone tone{4'096};
    std::array<std::int16_t, 8> buffer{};

    tone.fillStereo(buffer);

    REQUIRE(buffer[0] == 0);
    REQUIRE(buffer[1] == 0);
    REQUIRE(buffer[2] == 799);
    REQUIRE(buffer[3] == 799);
    REQUIRE(buffer[4] == 1567);
    REQUIRE(buffer[5] == 1567);
    REQUIRE(buffer[6] == 2276);
    REQUIRE(buffer[7] == 2276);
}

TEST_CASE("TestTone keeps phase between buffers")
{
    galerna::core::TestTone tone{4'096};
    std::array<std::int16_t, 4> first{};
    std::array<std::int16_t, 4> second{};

    tone.fillStereo(first);
    tone.fillStereo(second);

    REQUIRE(first[0] == 0);
    REQUIRE(first[2] == 799);
    REQUIRE(second[0] == 1567);
    REQUIRE(second[2] == 2276);
}
