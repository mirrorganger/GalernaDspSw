#include "galerna/effects/Grain.hpp"
#include "galerna/effects/GrainBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{

galerna::effects::GrainBuffer<64U> makeConstantBuffer(float value)
{
    galerna::effects::GrainBuffer<64U> buffer;
    buffer.init();
    for (int i = 0; i < 64; ++i)
    {
        buffer.write(value);
    }
    return buffer;
}

} // namespace

TEST_CASE("Grain is inactive and silent until triggered")
{
    galerna::effects::Grain grain;
    const auto buffer = makeConstantBuffer(1.0F);

    REQUIRE_FALSE(grain.isActive());
    REQUIRE(grain.process(buffer) == 0.0F);
}

TEST_CASE("Grain envelope starts and ends near zero, no clicks at either boundary")
{
    galerna::effects::Grain grain;
    const auto buffer = makeConstantBuffer(1.0F);
    constexpr std::uint32_t duration{100U};
    grain.trigger(10.0F, 1.0F, duration);

    const float first = grain.process(buffer);
    REQUIRE(std::abs(first) < 0.01F);

    float last{0.0F};
    for (std::uint32_t sample = 1U; sample < duration; ++sample)
    {
        last = grain.process(buffer);
    }
    REQUIRE(std::abs(last) < 0.2F);
}

TEST_CASE("Grain goes inactive exactly after durationSamples")
{
    galerna::effects::Grain grain;
    const auto buffer = makeConstantBuffer(1.0F);
    constexpr std::uint32_t duration{50U};
    grain.trigger(5.0F, 1.0F, duration);

    for (std::uint32_t sample = 0U; sample < duration - 1U; ++sample)
    {
        grain.process(buffer);
        REQUIRE(grain.isActive());
    }
    grain.process(buffer);
    REQUIRE_FALSE(grain.isActive());
}

TEST_CASE("Grain envelope rises above half amplitude near the middle of its duration")
{
    galerna::effects::Grain grain;
    const auto buffer = makeConstantBuffer(1.0F);
    constexpr std::uint32_t duration{100U};
    grain.trigger(10.0F, 1.0F, duration);

    float peak{0.0F};
    for (std::uint32_t sample = 0U; sample < duration; ++sample)
    {
        peak = std::max(peak, std::abs(grain.process(buffer)));
    }
    REQUIRE(peak > 0.5F);
}

TEST_CASE("Grain playbackRate away from 1 changes which buffer content is read over time")
{
    galerna::effects::GrainBuffer<128U> buffer;
    buffer.init();
    for (int i = 0; i < 128; ++i)
    {
        buffer.write(static_cast<float>(i)); // ramp, so read position is distinguishable
    }

    galerna::effects::Grain unityRate;
    galerna::effects::Grain fastRate;
    constexpr std::uint32_t duration{40U};
    constexpr float startDelay{30.0F};
    unityRate.trigger(startDelay, 1.0F, duration);
    fastRate.trigger(startDelay, 2.0F, duration);

    float unityMid{0.0F};
    float fastMid{0.0F};
    for (std::uint32_t sample = 0U; sample < duration / 2U; ++sample)
    {
        unityMid = unityRate.process(buffer);
        fastMid = fastRate.process(buffer);
    }
    REQUIRE(unityMid != fastMid);
}
