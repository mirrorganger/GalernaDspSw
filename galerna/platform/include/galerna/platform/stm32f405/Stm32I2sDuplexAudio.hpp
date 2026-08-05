#pragma once

#include "stm32f4xx_hal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace galerna::platform::stm32f405
{

namespace detail
{

using I2sDuplexCallback = void (*)(void* context);

struct I2sDuplexCallbacks
{
    I2sDuplexCallback onHalfComplete{nullptr};
    I2sDuplexCallback onComplete{nullptr};
    I2sDuplexCallback onError{nullptr};
    void* context{nullptr};
};

// One audio engine is ever active on real hardware, so a single registered set of callbacks
// (rather than a general registry) is enough to bridge the free-function HAL ISR callbacks
// below to whichever Stm32I2sDuplexAudio instance is running.
inline I2sDuplexCallbacks& activeI2sDuplexCallbacks()
{
    static I2sDuplexCallbacks callbacks{};
    return callbacks;
}

} // namespace detail

// Owns the interleaved stereo int16 DMA ping-pong buffers for full-duplex I2S2/I2S2ext audio
// and dispatches each completed half to Processor::process(rxHalf, txHalf). Processor is
// expected to look like core::DuplexAudioBlockProcessor. Not host-tested: this is a thin HAL
// wrapper with no logic of its own (the ping-pong bookkeeping is exercised via the Processor's
// own host tests), per the Platform/Stm32F405 testability rule in CLAUDE.md.
template <std::size_t FramesPerHalf, typename Processor>
class Stm32I2sDuplexAudio
{
public:
    Stm32I2sDuplexAudio(I2S_HandleTypeDef& handle, Processor& processor)
        : _handle{handle}
        , _processor{processor}
    {
    }

    bool start()
    {
        auto& callbacks = detail::activeI2sDuplexCallbacks();
        callbacks.context = this;
        callbacks.onHalfComplete = &dispatchHalfComplete;
        callbacks.onComplete = &dispatchComplete;
        callbacks.onError = &dispatchError;

        // DWT cycle counter: lets processHalf() measure exactly how many CPU cycles each
        // block's DSP work takes, to check it against the real-time budget (see
        // docs/progress.md for why that budget turned out to matter here).
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

        return HAL_I2SEx_TransmitReceive_DMA(
                   &_handle,
                   reinterpret_cast<std::uint16_t*>(_txBuffer.data()),
                   reinterpret_cast<std::uint16_t*>(_rxBuffer.data()),
                   static_cast<std::uint16_t>(samplesPerBuffer))
            == HAL_OK;
    }

    // Called from HAL_I2SEx_TxRxHalfCpltCallback: DMA just finished the first half, so it's
    // now safe to read the rx samples it captured there and refill the tx samples it will
    // transmit next time it wraps around to that half.
    void onHalfComplete()
    {
        processHalf(0U);
    }

    // Called from HAL_I2SEx_TxRxCpltCallback: DMA just finished the second half and wrapped.
    void onComplete()
    {
        processHalf(samplesPerHalf);
    }

    std::uint32_t errorCount() const
    {
        return _errorCount;
    }

    // Total onHalfComplete()/onComplete() invocations. Diagnostic: proves whether the DMA
    // callback chain is actually being invoked at all, independent of whether audio is audible.
    std::uint32_t callCount() const
    {
        return _callCount;
    }

    // Worst-case CPU cycles _processor.process() has taken for one half-buffer, measured via the
    // DWT cycle counter. Compare against FramesPerHalf/sampleRate * SystemCoreClock (the real
    // deadline for one half) to see how much of the ISR's time budget the DSP work is using.
    std::uint32_t maxProcessCycles() const
    {
        return _maxProcessCycles;
    }

private:
    static constexpr std::size_t samplesPerHalf{FramesPerHalf * 2U};
    static constexpr std::size_t samplesPerBuffer{samplesPerHalf * 2U};

    void processHalf(std::size_t offset)
    {
        ++_callCount;
        const std::uint32_t cyclesBefore = DWT->CYCCNT;
        _processor.process(
            std::span<const std::int16_t>{_rxBuffer.data() + offset, samplesPerHalf},
            std::span<std::int16_t>{_txBuffer.data() + offset, samplesPerHalf});
        const std::uint32_t elapsed = DWT->CYCCNT - cyclesBefore;
        if (elapsed > _maxProcessCycles)
        {
            _maxProcessCycles = elapsed;
        }
    }

    static void dispatchHalfComplete(void* context)
    {
        static_cast<Stm32I2sDuplexAudio*>(context)->onHalfComplete();
    }

    static void dispatchComplete(void* context)
    {
        static_cast<Stm32I2sDuplexAudio*>(context)->onComplete();
    }

    static void dispatchError(void* context)
    {
        ++static_cast<Stm32I2sDuplexAudio*>(context)->_errorCount;
    }

    I2S_HandleTypeDef& _handle;
    Processor& _processor;
    std::array<std::int16_t, samplesPerBuffer> _txBuffer{};
    std::array<std::int16_t, samplesPerBuffer> _rxBuffer{};
    std::uint32_t _errorCount{};
    std::uint32_t _callCount{};
    std::uint32_t _maxProcessCycles{};
};

} // namespace galerna::platform::stm32f405

// The HAL_I2SEx_TxRx*CpltCallback/HAL_I2S_ErrorCallback overrides live in
// Stm32I2sDuplexAudioCallbacks.cpp as ordinary (non-inline) functions, not here. HAL declares
// its default versions `__weak` specifically so a strong override replaces them; a C++ `inline`
// definition in a header only gets vague/weak linkage itself, so a weak-vs-weak tie at link time
// isn't guaranteed to pick ours over HAL's do-nothing default.
