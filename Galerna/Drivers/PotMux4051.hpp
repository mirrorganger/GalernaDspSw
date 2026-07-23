#pragma once

#include "Galerna/Hal/AdcConcept.hpp"
#include "Galerna/Hal/GpioConcept.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <ranges>
#include <utility>

namespace galerna::drivers
{

namespace detail
{

template <std::size_t... Bits>
constexpr std::array<bool, sizeof...(Bits)> toBitArray(std::uint8_t value, std::index_sequence<Bits...>)
{
    return {(((value >> Bits) & 1U) != 0U)...};
}

template <std::size_t SelectLineCount, std::size_t... Channels>
constexpr std::array<std::array<bool, SelectLineCount>, sizeof...(Channels)> makeChannelBitsTable(
    std::index_sequence<Channels...>)
{
    return {toBitArray(static_cast<std::uint8_t>(Channels), std::make_index_sequence<SelectLineCount>{})...};
}

} // namespace detail

template <hal::Adc TAdc, hal::Gpio TGpio>
class PotMux4051
{
public:
    static constexpr std::size_t selectLineCount{3};
    static constexpr std::size_t channelCount{std::size_t{1} << selectLineCount};

    PotMux4051(TAdc& adc, std::array<std::reference_wrapper<TGpio>, selectLineCount> selectLines, std::uint8_t adcChannel)
        : _adc{adc}
        , _selectLines{selectLines}
        , _adcChannel{adcChannel}
    {
    }

    std::uint16_t read(std::uint8_t muxChannel)
    {
        select(muxChannel);
        return _adc.read(_adcChannel);
    }

    void select(std::uint8_t muxChannel)
    {
        for (auto [line, bit] : std::views::zip(_selectLines, channelBitsTable[muxChannel]))
        {
            line.get().set(bit);
        }
    }

private:
    static constexpr auto channelBitsTable{
        detail::makeChannelBitsTable<selectLineCount>(std::make_index_sequence<channelCount>{})};

    TAdc& _adc;
    std::array<std::reference_wrapper<TGpio>, selectLineCount> _selectLines;
    std::uint8_t _adcChannel{};
};

} // namespace galerna::drivers
