#pragma once

namespace galerna::effects
{

class Gain
{
public:
    void setGain(float gain)
    {
        gain_ = gain;
    }

    template <typename AudioBuffer>
    void processBlock(AudioBuffer& buffer)
    {
        for (auto& sample : buffer.left)
        {
            sample *= gain_;
        }

        for (auto& sample : buffer.right)
        {
            sample *= gain_;
        }
    }

private:
    float gain_{1.0F};
};

} // namespace galerna::effects
