#include "Galerna/Core/AudioBuffer.hpp"
#include "Galerna/Core/ProcessorChain.hpp"
#include "Galerna/Effects/Gain.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("ProcessorChain applies gain to stereo buffer")
{
    std::array<float, 2> left{1.0F, 2.0F};
    std::array<float, 2> right{3.0F, 4.0F};

    galerna::core::AudioBuffer buffer{left, right};

    galerna::effects::Gain gain;
    gain.setGain(2.0F);

    galerna::core::ProcessorChain chain{gain};
    chain.processBlock(buffer);

    REQUIRE(left[0] == 2.0F);
    REQUIRE(left[1] == 4.0F);
    REQUIRE(right[0] == 6.0F);
    REQUIRE(right[1] == 8.0F);
}
