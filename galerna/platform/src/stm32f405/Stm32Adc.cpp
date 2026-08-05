#include "galerna/platform/stm32f405/Stm32Adc.hpp"

namespace galerna::platform::stm32f405
{

Stm32Adc::Stm32Adc(ADC_HandleTypeDef& handle)
    : handle_{handle}
{
}

std::uint16_t Stm32Adc::read(std::uint8_t channel)
{
    ADC_ChannelConfTypeDef sConfig{};
    sConfig.Channel = static_cast<std::uint32_t>(channel); // ADC_CHANNEL_n encodes n directly.
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    if (HAL_ADC_ConfigChannel(&handle_, &sConfig) != HAL_OK)
    {
        return 0U;
    }

    // Let the external mux + RC filter (1k / 100nF, ~100us tau) settle onto the
    // newly selected channel before sampling; 3 ADC cycles alone isn't enough.
    HAL_Delay(1U);

    if (HAL_ADC_Start(&handle_) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_ADC_PollForConversion(&handle_, 10U) != HAL_OK)
    {
        HAL_ADC_Stop(&handle_);
        return 0U;
    }

    const auto value = static_cast<std::uint16_t>(HAL_ADC_GetValue(&handle_));
    HAL_ADC_Stop(&handle_);
    return value;
}

} // namespace galerna::platform::stm32f405
