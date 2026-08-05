#include "galerna/core/DuplexAudioBlockProcessor.hpp"
#include "galerna/core/ProcessorChain.hpp"
#include "galerna/effects/Bypass.hpp"
#include "galerna/effects/Gain.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

TEST_CASE("DuplexAudioBlockProcessor passes audio through unchanged with Bypass")
{
    galerna::effects::Bypass bypass;
    galerna::core::ProcessorChain chain{bypass};
    galerna::core::DuplexAudioBlockProcessor<2, galerna::effects::Bypass> processor{chain};

    const std::array<std::int16_t, 4> rx{100, -200, 32'767, -32'768};
    std::array<std::int16_t, 4> tx{};

    processor.process(rx, tx);

    REQUIRE(tx == rx);
}

TEST_CASE("DuplexAudioBlockProcessor applies Gain and clamps on overflow")
{
    galerna::effects::Gain gain;
    gain.setGain(2.0F);
    galerna::core::ProcessorChain chain{gain};
    galerna::core::DuplexAudioBlockProcessor<2, galerna::effects::Gain> processor{chain};

    const std::array<std::int16_t, 4> rx{1'000, -1'000, 32'767, -32'768};
    std::array<std::int16_t, 4> tx{};

    processor.process(rx, tx);

    REQUIRE(tx[0] == 2'000);
    REQUIRE(tx[1] == -2'000);
    REQUIRE(tx[2] == 32'767);  // clamped, would otherwise overflow past int16 max
    REQUIRE(tx[3] == -32'768); // clamped, would otherwise overflow past int16 min
    REQUIRE(processor.clipCount() == 2);
}

TEST_CASE("DuplexAudioBlockProcessor clipCount stays zero when nothing clips")
{
    galerna::effects::Bypass bypass;
    galerna::core::ProcessorChain chain{bypass};
    galerna::core::DuplexAudioBlockProcessor<2, galerna::effects::Bypass> processor{chain};

    const std::array<std::int16_t, 4> rx{100, -200, 32'767, -32'768};
    std::array<std::int16_t, 4> tx{};

    processor.process(rx, tx);

    REQUIRE(processor.clipCount() == 0);
}
