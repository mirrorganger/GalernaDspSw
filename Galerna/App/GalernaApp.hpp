#pragma once


#include <array>
#include <cstdint>
#include <functional>
#include "Galerna/Hal/GpioConcept.hpp"

namespace galerna::app
{

template <hal::Gpio TStatusLed>
class GalernaApp
{
public:
    explicit GalernaApp(std::array<std::reference_wrapper<TStatusLed>, 3> statusLeds)
        : _statusLeds{statusLeds}
    {
    }

    void init()
    {
        for (auto& statusLed : _statusLeds)
        {
            statusLed.get().set(false);
        }
    }

    void tick()
    {
        for (std::size_t i = 0; i < _statusLeds.size(); ++i)
        {
            const bool value = static_cast<bool>((_currentStep >> i) & 0x1u);
            _statusLeds[i].get().set(value);
        }

        _currentStep = (_currentStep + 1) & ((1u << _statusLeds.size()) - 1u);
    }

private:
    std::uint8_t _currentStep{0};
    std::array<std::reference_wrapper<TStatusLed>, 3> _statusLeds;

};

} // namespace galerna::app
