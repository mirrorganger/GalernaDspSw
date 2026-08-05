#include "galerna/platform/stm32f405/Stm32I2sDuplexAudio.hpp"

// Ordinary (non-inline) definitions, deliberately not in the header -- see the comment left in
// its place in Stm32I2sDuplexAudio.hpp for why these must have strong linkage.

extern "C" void HAL_I2SEx_TxRxHalfCpltCallback(I2S_HandleTypeDef*)
{
    const auto& callbacks = galerna::platform::stm32f405::detail::activeI2sDuplexCallbacks();
    if (callbacks.onHalfComplete != nullptr)
    {
        callbacks.onHalfComplete(callbacks.context);
    }
}

extern "C" void HAL_I2SEx_TxRxCpltCallback(I2S_HandleTypeDef*)
{
    const auto& callbacks = galerna::platform::stm32f405::detail::activeI2sDuplexCallbacks();
    if (callbacks.onComplete != nullptr)
    {
        callbacks.onComplete(callbacks.context);
    }
}

extern "C" void HAL_I2S_ErrorCallback(I2S_HandleTypeDef*)
{
    const auto& callbacks = galerna::platform::stm32f405::detail::activeI2sDuplexCallbacks();
    if (callbacks.onError != nullptr)
    {
        callbacks.onError(callbacks.context);
    }
}
