#pragma once

#include "galerna/effects/ModulatedDelayLine.hpp"

#include <cstddef>

namespace galerna::effects
{

// One Schroeder allpass "diffuser" stage: recirculates through a ModulatedDelayLine with
// feedback, using the classic single-multiply topology (see e.g. CCRMA's notes on Schroeder
// allpass sections). Chaining a few of these in series is what "thickens" a reverb's early
// reflections/late tail into a smooth wash instead of a sparse set of echoes (see
// docs/architecture.md's Reverb section). feedback == 0 degenerates to a plain modulated delay
// tap -- CloudReverbLine relies on this to reuse the same modulation math for its own delay, via
// ModulatedDelayLine directly, since that needs separate read/write control this class's coupled
// process() doesn't offer.
template <std::size_t BufferSize>
class ModulatedAllpass
{
public:
    void init(float sampleRate)
    {
        _line.init(sampleRate);
    }

    void setDelay(float baseDelayS, float modDepthS, float modRateHz)
    {
        _line.setDelay(baseDelayS, modDepthS, modRateHz);
    }

    // feedback: 0..~0.7 in practice ("diffusion amount" -- higher smears the signal more, but
    // pushes this stage's own recirculating loop closer to instability). Values >= 1 are not
    // clamped here; callers are expected to pass a sane, stability-tested constant (this project
    // only ever configures this from fixed tuned constants in CloudReverb.hpp, never from a live
    // pot).
    void setFeedback(float feedback)
    {
        _feedback = feedback;
    }

    // See ModulatedDelayLine's always_inline comment -- same reasoning, same hot path.
    [[gnu::always_inline]] float process(float input)
    {
        const float delayed = _line.readDelayed();
        const float recirculated = input + _feedback * delayed;
        _line.write(recirculated);
        return delayed - _feedback * recirculated;
    }

private:
    ModulatedDelayLine<BufferSize> _line;
    float _feedback{0.5F};
};

} // namespace galerna::effects
