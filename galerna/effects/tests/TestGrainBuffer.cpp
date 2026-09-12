#include "galerna/effects/GrainBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("GrainBuffer readAt(0) returns the most recently written sample")
{
    galerna::effects::GrainBuffer<8U> buffer;
    buffer.init();

    buffer.write(1.0F);
    buffer.write(2.0F);
    buffer.write(3.0F);

    REQUIRE(buffer.readAt(0.0F) == 3.0F);
}

TEST_CASE("GrainBuffer readAt recovers a previously written sample at its known delay")
{
    galerna::effects::GrainBuffer<8U> buffer;
    buffer.init();

    buffer.write(10.0F);
    buffer.write(20.0F);
    buffer.write(30.0F);
    buffer.write(40.0F);

    // 40 was just written (delay 0); 10 was written 3 writes ago (delay 3).
    REQUIRE(buffer.readAt(3.0F) == 10.0F);
    REQUIRE(buffer.readAt(1.0F) == 30.0F);
}

TEST_CASE("GrainBuffer readAt interpolates between neighboring samples for a fractional delay")
{
    galerna::effects::GrainBuffer<8U> buffer;
    buffer.init();

    buffer.write(0.0F);
    buffer.write(10.0F);

    // Halfway between the two most recent writes (10.0 at delay 0, 0.0 at delay 1).
    REQUIRE(buffer.readAt(0.5F) == 5.0F);
}

TEST_CASE("GrainBuffer readAt clamps an out-of-range delay instead of reading out of bounds")
{
    galerna::effects::GrainBuffer<4U> buffer;
    buffer.init();

    buffer.write(1.0F);
    buffer.write(2.0F);
    buffer.write(3.0F);
    buffer.write(4.0F);

    REQUIRE(buffer.readAt(-1.0F) == buffer.readAt(0.0F));
    REQUIRE(buffer.readAt(1'000.0F) == buffer.readAt(3.0F));
}
