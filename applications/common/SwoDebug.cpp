#include "stm32f4xx_hal.h"

#include <cstdint>

namespace
{

constexpr auto swoBaudRate = std::uint32_t{2'000'000};

void enableSwoTrace()
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DBGMCU->CR = (DBGMCU->CR & ~DBGMCU_CR_TRACE_MODE_Msk) | DBGMCU_CR_TRACE_IOEN;

    ITM->LAR = 0xC5ACCE55U;
    TPI->ACPR = (SystemCoreClock / swoBaudRate) - 1U;
    TPI->SPPR = 2U; // NRZ / UART SWO.
    TPI->FFCR = 0U; // Disable formatter for raw ITM stimulus output.

    ITM->TPR = 0U;
    ITM->TER = 1U;
    ITM->TCR = ITM_TCR_ITMENA_Msk |
               ITM_TCR_SWOENA_Msk |
               ITM_TCR_SYNCENA_Msk |
               (1U << ITM_TCR_TraceBusID_Pos);
}

} // namespace

extern "C" int __io_putchar(int ch)
{
    enableSwoTrace();
    ITM_SendChar(static_cast<std::uint32_t>(ch));
    return ch;
}
