#pragma once

#include "Galerna/Hal/AdcConcept.hpp"
#include "Galerna/Hal/GpioConcept.hpp"

#include <cstdint>

namespace galerna::drivers
{

template <hal::Adc TAdc, hal::Gpio TS0, hal::Gpio TS1, hal::Gpio TS2>
class PotMux4051
{
public:
    PotMux4051(TAdc& adc, TS0& s0, TS1& s1, TS2& s2, std::uint8_t adcChannel)
        : adc_{adc}
        , s0_{s0}
        , s1_{s1}
        , s2_{s2}
        , adcChannel_{adcChannel}
    {
    }

    std::uint16_t read(std::uint8_t muxChannel)
    {
        select(muxChannel);
        return adc_.read(adcChannel_);
    }

    void select(std::uint8_t muxChannel)
    {
        s0_.set((muxChannel & 0x01U) != 0U);
        s1_.set((muxChannel & 0x02U) != 0U);
        s2_.set((muxChannel & 0x04U) != 0U);
    }

private:
    TAdc& adc_;
    TS0& s0_;
    TS1& s1_;
    TS2& s2_;
    std::uint8_t adcChannel_{};
};

} // namespace galerna::drivers
