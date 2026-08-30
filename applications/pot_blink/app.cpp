#include "app.h"

#include "BoardControls.h"
#include "PotBlinkApp.hpp"

#include "main.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace
{

galerna::app::BoardControls boardControls{galerna::app::makeBoardControls()};
auto potMux = galerna::app::makePotMux(boardControls);

// LED0/1/2 blink at a rate driven by POT_1/2/3 respectively.
constexpr std::array<std::uint8_t, 3> ledPotMuxChannels{2U, 4U, 0U};

constexpr std::uint32_t appTickIntervalMs{20U};
constexpr std::uint32_t potPrintIntervalTicks{50U}; // ~1 s at appTickIntervalMs

galerna::app::PotBlinkApp app{
    boardControls.statusLeds,
    potMux,
    ledPotMuxChannels,
    boardControls.buttons,
    boardControls.switches,
    appTickIntervalMs};

void printPotValues()
{
    std::printf("Pots:");
    for (std::uint8_t channel = 0U; channel < galerna::app::potMuxChannelCount; ++channel)
    {
        std::printf(
            " %s=%u", galerna::app::potMuxChannelName[channel], static_cast<unsigned>(potMux.read(channel)));
    }
    std::printf("\r\n");
}

} // namespace

extern "C" void App_Init(void)
{
    app.init();
    std::printf("Galerna SWO printf ready; SystemCoreClock=%lu Hz\r\n", static_cast<unsigned long>(SystemCoreClock));
    std::printf("Pot-driven LED blink demo started\r\n");
}

extern "C" void App_Tick(void)
{
    static auto tickCount = std::uint32_t{};

    app.tick();

    ++tickCount;
    if ((tickCount % potPrintIntervalTicks) == 0U)
    {
        printPotValues();
    }

    HAL_Delay(appTickIntervalMs);
}
