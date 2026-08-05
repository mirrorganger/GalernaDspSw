#pragma once

namespace galerna::effects
{

class Bypass
{
public:
    template <typename AudioBuffer>
    void processBlock(AudioBuffer&)
    {
    }
};

} // namespace galerna::effects
