#pragma once

#include "Galerna/Hal/GpioConcept.hpp"

namespace galerna::app
{

template <hal::Gpio TStatusLed>
class GalernaApp
{
public:
    explicit GalernaApp(TStatusLed& statusLed)
        : statusLed_{statusLed}
    {
    }

    void init()
    {
        statusLed_.set(false);
    }

    void tick()
    {
        statusLed_.set(!statusLed_.get());
    }

private:
    TStatusLed& statusLed_;
};

} // namespace galerna::app
