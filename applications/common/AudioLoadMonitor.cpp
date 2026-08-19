#include "AudioLoadMonitor.h"

#include "main.h"

#include <cstdio>

namespace galerna::app
{

void printAudioLoad(
    std::size_t audioFramesPerHalf,
    float sampleRateHz,
    std::uint32_t callCount,
    std::uint32_t errorCount,
    std::uint32_t maxProcessCycles,
    std::uint32_t clipCount)
{
    const auto budgetCycles = static_cast<std::uint32_t>(
        static_cast<float>(SystemCoreClock) * static_cast<float>(audioFramesPerHalf) / sampleRateHz);
    const std::uint32_t budgetPercentTenths = budgetCycles > 0U
        ? static_cast<std::uint32_t>((static_cast<std::uint64_t>(maxProcessCycles) * 1000U) / budgetCycles)
        : 0U;
    std::printf(
        "Audio DMA: callCount=%lu errorCount=%lu maxProcessCycles=%lu/%lu (%lu.%lu%%) clipCount=%lu\r\n",
        static_cast<unsigned long>(callCount),
        static_cast<unsigned long>(errorCount),
        static_cast<unsigned long>(maxProcessCycles),
        static_cast<unsigned long>(budgetCycles),
        static_cast<unsigned long>(budgetPercentTenths / 10U),
        static_cast<unsigned long>(budgetPercentTenths % 10U),
        static_cast<unsigned long>(clipCount));
}

} // namespace galerna::app
