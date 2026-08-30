#pragma once

#include <cstddef>
#include <cstdint>

namespace galerna::app
{

// Prints the SWO diagnostic line every audio app uses to watch its real-time CPU headroom: how
// many DWT cycles one half-buffer's worth of real time allows (audioFramesPerHalf / sampleRateHz,
// converted via SystemCoreClock) vs. the worst-case cycles the DSP work has actually taken
// (maxProcessCycles, from Stm32I2sDuplexAudio's DWT cycle counter), plus the DMA call/error
// counts and the block processor's clip count. Percentage is printed as an integer-math
// percentage (tenths) to avoid relying on newlib-nano's optional float printf support.
void printAudioLoad(
    std::size_t audioFramesPerHalf,
    float sampleRateHz,
    std::uint32_t callCount,
    std::uint32_t errorCount,
    std::uint32_t maxProcessCycles,
    std::uint32_t clipCount);

} // namespace galerna::app
