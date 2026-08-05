#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace galerna::core
{

// Chamberlin state-variable filter: a 2-pole resonant lowpass. The topology also produces
// bandpass/highpass internally, but only the lowpass output is exposed -- this project only
// needs a resonant lowpass for effect "timbre" character.
class StateVariableFilter
{
public:
    void init(float sampleRate)
    {
        _sampleRate = sampleRate;
        _lowpass = 0.0F;
        _bandpass = 0.0F;
    }

    // cutoffHz: filter corner frequency. Clamped well below sampleRate/4 -- the Chamberlin
    // coefficient (f = 2*sin(pi*cutoffHz/sampleRate)) is a small-angle approximation that loses
    // accuracy and eventually stability as cutoffHz approaches sampleRate/4.
    void setCutoff(float cutoffHz)
    {
        const float clampedHz = std::clamp(cutoffHz, 20.0F, _sampleRate / 4.0F);
        _f = 2.0F * std::sin(std::numbers::pi_v<float> * clampedHz / _sampleRate);
    }

    // resonance: 0..1, 0 = heavily damped, 1 = near self-oscillation. Internally this is the
    // inverse-Q damping term; clamped away from 0 to guarantee stability.
    void setResonance(float resonance)
    {
        constexpr float minQ{0.05F};
        constexpr float maxQ{2.0F};
        _q = maxQ - std::clamp(resonance, 0.0F, 1.0F) * (maxQ - minQ);
    }

    float process(float input)
    {
        _lowpass += _f * _bandpass;
        const float highpass = input - _lowpass - _q * _bandpass;
        _bandpass += _f * highpass;

        // Defensive clamp: guards against numerical blow-up from parameter combinations at the
        // edge of the small-angle approximation's validity, well outside the signal's expected
        // +-1ish range so it never affects normal operation.
        constexpr float stateLimit{8.0F};
        _lowpass = std::clamp(_lowpass, -stateLimit, stateLimit);
        _bandpass = std::clamp(_bandpass, -stateLimit, stateLimit);

        return _lowpass;
    }

private:
    float _sampleRate{1.0F};
    float _f{0.0F};
    float _q{2.0F};
    float _lowpass{0.0F};
    float _bandpass{0.0F};
};

} // namespace galerna::core
