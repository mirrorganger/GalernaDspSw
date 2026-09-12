#include "galerna/core/AudioBuffer.hpp"
#include "galerna/effects/GranularCloud.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("GranularCloud is silent with density at zero and no manual retrigger")
{
    galerna::effects::GranularCloud cloud;
    cloud.init(48'000.0F);
    cloud.setDensity(0.0F);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    galerna::core::AudioBuffer buffer{left, right};

    cloud.processBlock(buffer);

    REQUIRE(cloud.activeGrainCount() == 0U);
    for (float sample : left)
    {
        REQUIRE(sample == 0.0F);
    }
}

TEST_CASE("GranularCloud produces sound once grains start scheduling")
{
    galerna::effects::GranularCloud cloud;
    cloud.init(48'000.0F);
    cloud.setDensity(1.0F);

    std::array<float, 8'192> left{};
    std::array<float, 8'192> right{};
    galerna::core::AudioBuffer buffer{left, right};

    cloud.processBlock(buffer);

    bool everNonzero = false;
    for (float sample : left)
    {
        if (sample != 0.0F)
        {
            everNonzero = true;
        }
    }
    REQUIRE(everNonzero);
}

TEST_CASE("GranularCloud writes an identical mono signal to both channels")
{
    galerna::effects::GranularCloud cloud;
    cloud.init(48'000.0F);
    cloud.setDensity(1.0F);

    std::array<float, 4'096> left{};
    std::array<float, 4'096> right{};
    galerna::core::AudioBuffer buffer{left, right};

    cloud.processBlock(buffer);

    REQUIRE(left == right);
}

TEST_CASE("GranularCloud retriggerAll immediately activates every pool slot")
{
    galerna::effects::GranularCloud cloud;
    cloud.init(48'000.0F);
    cloud.setDensity(0.0F);

    cloud.retriggerAll();

    REQUIRE(cloud.activeGrainCount() == galerna::effects::GranularCloud::grainPoolSize);
}

TEST_CASE("GranularCloud setFrozen keeps grains sounding from a fixed snapshot")
{
    galerna::effects::GranularCloud cloud;
    cloud.init(48'000.0F);
    cloud.setDensity(0.0F);

    std::array<float, 1'024> left{};
    std::array<float, 1'024> right{};
    galerna::core::AudioBuffer buffer{left, right};

    // Populate the grain buffer with real source material before freezing -- otherwise "frozen"
    // just means "frozen on silence", which wouldn't distinguish this from a bug.
    cloud.processBlock(buffer);
    cloud.setFrozen(true);
    cloud.retriggerAll();

    cloud.processBlock(buffer);

    bool everNonzero = false;
    for (float sample : left)
    {
        if (sample != 0.0F)
        {
            everNonzero = true;
        }
    }
    REQUIRE(everNonzero);
}

TEST_CASE("GranularCloud output stays bounded and finite at full resonance and pitch spread")
{
    galerna::effects::GranularCloud cloud;
    cloud.init(48'000.0F);
    cloud.setDensity(1.0F);
    cloud.setResonance(1.0F);
    cloud.setPitchSpread(1.0F);
    cloud.setSpray(1.0F);

    std::array<float, 48'000> left{};
    std::array<float, 48'000> right{};
    galerna::core::AudioBuffer buffer{left, right};

    cloud.processBlock(buffer);

    for (float sample : left)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 8.0F);
    }
}

TEST_CASE("GranularCloud timbre changes the generated signal")
{
    galerna::effects::GranularCloud cloudDark;
    cloudDark.init(48'000.0F);
    cloudDark.setDensity(1.0F);
    cloudDark.setTimbre(0.02F);

    galerna::effects::GranularCloud cloudBright;
    cloudBright.init(48'000.0F);
    cloudBright.setDensity(1.0F);
    cloudBright.setTimbre(0.98F);

    std::array<float, 4'096> leftDark{};
    std::array<float, 4'096> rightDark{};
    std::array<float, 4'096> leftBright{};
    std::array<float, 4'096> rightBright{};
    galerna::core::AudioBuffer bufferDark{leftDark, rightDark};
    galerna::core::AudioBuffer bufferBright{leftBright, rightBright};

    cloudDark.processBlock(bufferDark);
    cloudBright.processBlock(bufferBright);

    REQUIRE(leftDark != leftBright);
}
