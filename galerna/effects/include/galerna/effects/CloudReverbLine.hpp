#pragma once

#include "galerna/effects/ModulatedAllpass.hpp"
#include "galerna/effects/ModulatedDelayLine.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace galerna::effects
{

// One of CloudReverb's parallel late-reverb "lines": a single modulated delay in a feedback
// loop, with a one-pole damping lowpass and a short chain of ModulatedAllpass diffuser stages
// inside that loop (see docs/architecture.md's Reverb section for the block diagram). Several of
// these summed together, each with a different delay time, is what turns a single metallic-
// sounding comb filter into a denser, more diffuse tail.
template <std::size_t DelayBufferSize, std::size_t DiffuserBufferSize, std::size_t DiffuserStageCount>
class CloudReverbLine
{
public:
    void init(float sampleRate, float delayS, float modDepthS, float modRateHz)
    {
        _delay.init(sampleRate);
        _delay.setDelay(delayS, modDepthS, modRateHz);
        _damped = 0.0F;

        for (std::size_t stage = 0U; stage < DiffuserStageCount; ++stage)
        {
            _diffuser[stage].init(sampleRate);
            _diffuser[stage].setDelay(
                diffuserBaseDelayS[stage % diffuserBaseDelayS.size()],
                diffuserModDepthS,
                diffuserModRateHz[stage % diffuserModRateHz.size()]);
            _diffuser[stage].setFeedback(diffuserFeedback);
        }
    }

    // feedback: 0..maxFeedback ("size"/decay length -- see CloudReverb::setSize()). Clamped below
    // 1 so this line's own recirculating loop (delay -> damping -> diffuser chain -> back into
    // the delay) always converges: damping and the diffuser stages are unity-gain-ish at best, so
    // feedback alone bounds the loop gain.
    void setFeedback(float feedback)
    {
        _feedback = std::clamp(feedback, 0.0F, maxFeedback);
    }

    // flatten: forces the whole per-sample call chain (delay/damping/diffuser stages) to inline
    // into one function body -- see ModulatedDelayLine's always_inline comment for why this
    // matters on this hardware.
    [[gnu::flatten]] float process(float input)
    {
        const float delayed = _delay.readDelayed();
        _damped += dampingCoeff * (delayed - _damped);

        float diffused = _damped;
        for (auto& stage : _diffuser)
        {
            diffused = stage.process(diffused);
        }

        _delay.write(input + diffused * _feedback);
        return diffused;
    }

private:
    // Stability margin: unlike a plain comb filter, this loop's gain also includes the diffuser
    // chain, so it's given a lower ceiling than a single feedback multiply would need.
    static constexpr float maxFeedback{0.92F};
    // Fixed damping (darkens the recirculating tail, like a real chime's air absorption) --
    // deliberately not exposed as a live control; see CloudReverb.hpp for which two parameters
    // (mix, size) this app actually wires up to pots.
    static constexpr float dampingCoeff{0.35F};
    static constexpr std::array<float, 2U> diffuserBaseDelayS{0.003F, 0.005F};
    static constexpr float diffuserModDepthS{0.0006F};
    static constexpr std::array<float, 2U> diffuserModRateHz{0.17F, 0.11F};
    static constexpr float diffuserFeedback{0.5F};

    ModulatedDelayLine<DelayBufferSize> _delay;
    std::array<ModulatedAllpass<DiffuserBufferSize>, DiffuserStageCount> _diffuser{};
    float _damped{0.0F};
    float _feedback{0.7F};
};

} // namespace galerna::effects
