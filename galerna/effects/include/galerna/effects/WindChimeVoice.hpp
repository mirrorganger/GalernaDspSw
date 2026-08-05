#pragma once

#include "galerna/core/WavetableOscillator.hpp"
#include "galerna/core/Xorshift32.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// One voice of a generative ambient "wind chime": sits silent, waits a randomized interval, then
// strikes a random note from a pentatonic scale (picked by its own Xorshift32 stream) and rings
// out with an exponential decay, like a physical chime struck by the wind -- then goes back to
// waiting. Unlike ThxVoice (which glides toward one shared, externally-driven target), each
// WindChimeVoice free-runs its own schedule once seeded, so a bank of them (see WindChimes.hpp)
// naturally decorrelates into an evolving, non-repeating texture instead of a fixed chord.
class WindChimeVoice
{
public:
    // Chime-register range: high enough to read as bell-like rather than the sub-bass end of
    // ThxVoice's range, low enough to stay clear of harshness at the top of the audible band.
    static constexpr float lowestFrequencyHz{220.0F};
    static constexpr float highestFrequencyHz{1'760.0F};

    // Ratios of a pentatonic scale (root, major second, major third, perfect fifth, major
    // sixth) relative to lowestFrequencyHz -- avoids semitone clashes between simultaneously
    // ringing voices, keeping the texture consonant regardless of which notes land together.
    static constexpr std::size_t scaleDegreeCount{5U};
    static constexpr std::array<float, scaleDegreeCount> scaleRatios{
        1.0F, 9.0F / 8.0F, 5.0F / 4.0F, 3.0F / 2.0F, 5.0F / 3.0F};

    // Deliberately leaves _samplesUntilNextStrike at 0 (state waiting) rather than calling
    // scheduleNextStrike() here: at construction time no update() has run yet, so there is no
    // real density/spread to schedule from -- rather than waiting out an arbitrary made-up
    // interval, the voice takes its first strike on the very first process() call (a chime
    // starts ringing as soon as the wind picks up) and only starts honoring density/spread for
    // scheduling once that first strike has rung out.
    void init(float sampleRate, std::uint32_t seed)
    {
        _sampleRate = sampleRate;
        _osc.init(sampleRate);
        _rng = core::Xorshift32{seed};
        _amplitude = 0.0F;
        _decayPerSample = 1.0F;
        _samplesUntilNextStrike = 0U;
        _state = State::waiting;
    }

    // density: 0..1, higher shortens the average wait between strikes.
    // spread: 0..1, higher widens how many octaves above the scale root a strike can land on.
    // ringDurationS: how long (in seconds) a struck note takes to decay from full amplitude down
    // to silenceThreshold; clamped away from 0 (division by ringSamples in strike()) and to a
    // sane upper bound regardless of what the caller passes in.
    void update(float density, float spread, float ringDurationS)
    {
        _density = std::clamp(density, 0.0F, 1.0F);
        _spread = std::clamp(spread, 0.0F, 1.0F);
        _ringDurationS = std::clamp(ringDurationS, minRingDurationS, maxRingDurationS);
    }

    [[gnu::always_inline]] float process()
    {
        if (_samplesUntilNextStrike > 0U)
        {
            --_samplesUntilNextStrike;
        }
        else if (_state == State::waiting)
        {
            strike();
        }

        _amplitude *= _decayPerSample;
        if (_state == State::ringing && _amplitude < silenceThreshold)
        {
            _amplitude = 0.0F;
            _state = State::waiting;
            scheduleNextStrike();
        }

        return _osc.process() * _amplitude;
    }

    // Frequency of the most recent strike (held after the voice rings back to silence, until
    // the next strike overwrites it).
    float frequencyHz() const
    {
        return _frequencyHz;
    }

private:
    enum class State
    {
        waiting,
        ringing
    };

    static constexpr float silenceThreshold{0.001F};
    static constexpr float minRingDurationS{0.05F};
    static constexpr float maxRingDurationS{6.0F};
    // minIntervalS only bounds the *wait* before a strike, not the strike-to-strike rate: a
    // voice can't re-strike until its current ring has decayed below silenceThreshold (see
    // process()), so at maximum density a single voice's real cycle time is close to
    // ringDurationS + a small jittered fraction of minIntervalS, not minIntervalS itself. The
    // Decay control (see WindChimes::setDecay()) shortens ringDurationS itself if a faster max
    // density should be audible on a single voice rather than just tightening how closely
    // staggered voices in the bank can land on top of each other.
    static constexpr float minIntervalS{0.05F};
    static constexpr float maxIntervalS{4.0F};
    // Highest octave a strike can be shifted up by, reached at spread == 1.
    static constexpr float maxOctaveSpread{2.0F};

    void strike()
    {
        const auto degree = static_cast<std::size_t>(_rng.nextFloat01() * static_cast<float>(scaleDegreeCount))
            % scaleDegreeCount;
        const auto octave = static_cast<float>(
            static_cast<int>(_rng.nextFloat01() * (1.0F + _spread * maxOctaveSpread)));
        const float frequencyHz = std::clamp(
            lowestFrequencyHz * scaleRatios[degree] * std::pow(2.0F, octave),
            lowestFrequencyHz,
            highestFrequencyHz);

        _osc.setFrequency(frequencyHz);
        _frequencyHz = frequencyHz;
        _amplitude = 1.0F;
        const float ringSamples = _ringDurationS * _sampleRate;
        _decayPerSample = std::pow(silenceThreshold, 1.0F / ringSamples);
        _state = State::ringing;
    }

    void scheduleNextStrike()
    {
        // Higher density -> shorter mean wait; a random jitter factor (0.5x-1.5x) keeps voices
        // from ever locking into a mechanical, repeating cadence.
        const float meanIntervalS = maxIntervalS - _density * (maxIntervalS - minIntervalS);
        const float jitter = 0.5F + _rng.nextFloat01();
        _samplesUntilNextStrike = static_cast<std::uint32_t>(meanIntervalS * jitter * _sampleRate);
    }

    core::WavetableOscillator<64U> _osc;
    core::Xorshift32 _rng{1U};
    float _sampleRate{1.0F};
    float _density{0.5F};
    float _spread{0.5F};
    float _ringDurationS{1.4F};
    float _amplitude{0.0F};
    float _decayPerSample{1.0F};
    float _frequencyHz{lowestFrequencyHz};
    std::uint32_t _samplesUntilNextStrike{0U};
    State _state{State::waiting};
};

} // namespace galerna::effects
