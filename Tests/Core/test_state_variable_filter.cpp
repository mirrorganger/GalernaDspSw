#include "Galerna/Core/StateVariableFilter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("StateVariableFilter lowpass output settles to a constant input (unity DC gain)")
{
    galerna::core::StateVariableFilter filter;
    filter.init(48'000.0F);
    filter.setCutoff(1'000.0F);
    filter.setResonance(0.0F);

    float output = 0.0F;
    for (int i = 0; i < 4'000; ++i)
    {
        output = filter.process(0.5F);
    }

    REQUIRE(std::abs(output - 0.5F) < 0.02F);
}

TEST_CASE("StateVariableFilter stays finite and bounded across a cutoff/resonance sweep")
{
    const std::array<float, 4> cutoffsHz{50.0F, 500.0F, 2'000.0F, 8'000.0F};
    const std::array<float, 4> resonances{0.0F, 0.3F, 0.7F, 1.0F};

    for (float cutoffHz : cutoffsHz)
    {
        for (float resonance : resonances)
        {
            galerna::core::StateVariableFilter filter;
            filter.init(32'552.0F);
            filter.setCutoff(cutoffHz);
            filter.setResonance(resonance);

            float input = 1.0F;
            for (int i = 0; i < 200; ++i)
            {
                const float output = filter.process(input);
                REQUIRE(std::isfinite(output));
                REQUIRE(std::abs(output) <= 8.0F);
                input = -input; // alternating full-scale input: a worst-case stress signal
            }
        }
    }
}

TEST_CASE("StateVariableFilter resonance changes the impulse response")
{
    galerna::core::StateVariableFilter lowResonance;
    lowResonance.init(48'000.0F);
    lowResonance.setCutoff(1'000.0F);
    lowResonance.setResonance(0.0F);

    galerna::core::StateVariableFilter highResonance;
    highResonance.init(48'000.0F);
    highResonance.setCutoff(1'000.0F);
    highResonance.setResonance(0.9F);

    std::array<float, 32> lowResonanceOutput{};
    std::array<float, 32> highResonanceOutput{};
    for (std::size_t i = 0; i < lowResonanceOutput.size(); ++i)
    {
        const float impulse = (i == 0U) ? 1.0F : 0.0F;
        lowResonanceOutput[i] = lowResonance.process(impulse);
        highResonanceOutput[i] = highResonance.process(impulse);
    }

    REQUIRE(lowResonanceOutput != highResonanceOutput);
}
