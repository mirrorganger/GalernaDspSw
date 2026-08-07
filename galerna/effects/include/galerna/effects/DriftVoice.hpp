#pragma once

#include "galerna/core/PentatonicScale.hpp"
#include "galerna/core/Range.hpp"
#include "galerna/core/WavetableOscillator.hpp"
#include "galerna/core/Xorshift32.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// One continuously-sounding oscillator voice for AmbientPad, self-drifting rather than externally
// gated (unlike PluckVoice): there is no noteOn()/noteOff(), no envelope -- process() always
// produces sound once init()'d. Two independent block-rate drift mechanisms are layered on top of
// a fixed per-voice detune ratio (see AmbientPad::setDetune()):
//
//  1. Slow harmonic drift: every noteChangeIntervalS seconds (jittered via its own Xorshift32, so
//     several voices don't lock into the same rhythm), picks a new pentatonic-quantized target
//     frequency (same quantization shape as PluckVoice::setPendingFrequency()) and glides toward
//     it with a one-pole filter, so the pad's chord slowly morphs rather than holding a static
//     drone or snapping between notes.
//  2. Continuous micro pitch wobble: a second, independent one-pole-smoothed random walk (own
//     Xorshift32, own retarget timer) around the current glide target -- the "tape wobble"/analog-
//     drift character. Same ratio-random-walk shape as TwinPluck::DroneWander, kept as its own
//     copy here rather than shared: that version decorates a gated voice at a fixed depth/rate,
//     this one decorates an always-on voice at pot-controlled depth/rate -- different enough call
//     sites that two small copies read clearer than one class parameterized for both.
//
// Both mechanisms are updated at block rate (once per updateDrift() call, not per sample) -- same
// reasoning as TwinPluck::updateWander(), whose block-rate wander cost only +34 cycles on real
// hardware.
class DriftVoice
{
public:
    static constexpr float rootFrequencyHz{110.0F};
    static constexpr std::size_t octaveRange{3U};

    void init(float sampleRate, std::uint32_t noteSeed, std::uint32_t wobbleSeed)
    {
        _sampleRate = sampleRate;
        _osc.init(sampleRate);
        _noteRng = core::Xorshift32{noteSeed};
        _wobbleRng = core::Xorshift32{wobbleSeed};
        _currentFrequencyHz = rootFrequencyHz;
        _targetFrequencyHz = rootFrequencyHz;
        _wobbleRatio = 1.0F;
        _wobbleTargetRatio = 1.0F;
        _samplesUntilNoteChange = 0U;
        _samplesUntilWobbleRetarget = 0U;
        _osc.setFrequency(rootFrequencyHz);
        setEvolveRate(0.5F);
        setDriftDepth(0.3F);
    }

    // ratio: fixed per-voice frequency multiplier (e.g. 0.995..1.005) set once by AmbientPad to
    // spread voices around a shared center for unison/chorus thickness -- see its setDetune().
    void setDetuneRatio(float ratio)
    {
        _detuneRatio = ratio;
    }

    // normalized: 0..1 -> 0..maxDriftDepth wobble ratio deviation around the current glide target.
    void setDriftDepth(float normalized)
    {
        _driftDepth = maxDriftDepth * std::clamp(normalized, 0.0F, 1.0F);
    }

    // normalized: 0..1, exponentially maps to how quickly the pad moves -- near 0 is glacial (up
    // to noteChangeSRange.max between chord changes), near 1 is restless (as fast as
    // noteChangeSRange.min). The fast tape-wobble retarget rate scales proportionally with the
    // same normalized value. Both ranges run the opposite way from Range::exponential()'s
    // 0->min/1->max shape, hence the 1 - clamped.
    void setEvolveRate(float normalized)
    {
        const float clamped = std::clamp(normalized, 0.0F, 1.0F);
        _noteChangeIntervalS = noteChangeSRange.exponential(1.0F - clamped);
        _wobbleRetargetS = wobbleRetargetSRange.exponential(1.0F - clamped);
    }

    // frozen: true stops picking new pentatonic targets (the chord holds still) without touching
    // the wobble layer, so the pad keeps breathing instead of going perfectly static.
    void setFrozen(bool frozen)
    {
        _frozen = frozen;
    }

    // Forces an immediate note re-target instead of waiting for the jittered timer -- a manual
    // "next chord" trigger (see AmbientPad::reseed()).
    void reseed()
    {
        _samplesUntilNoteChange = 0U;
    }

    // blockSamples: the AudioBuffer size passed to the current processBlock() call -- see this
    // class's comment for why both drift mechanisms are updated here, once per block, rather than
    // per sample.
    void updateDrift(std::size_t blockSamples)
    {
        if (!_frozen)
        {
            if (_samplesUntilNoteChange <= blockSamples)
            {
                constexpr std::size_t stepCount{core::PentatonicScale::degreeCount * octaveRange};
                const std::size_t step = static_cast<std::size_t>(_noteRng.next()) % stepCount;
                const std::size_t degree = step % core::PentatonicScale::degreeCount;
                const auto octave  = static_cast<float>(step / core::PentatonicScale::degreeCount);
                _targetFrequencyHz = rootFrequencyHz * core::PentatonicScale::ratios[degree] * std::pow(2.0F, octave);
                const float jitter = 0.5F + _noteRng.nextFloat01(); // 0.5x-1.5x, avoids a locked cadence
                _samplesUntilNoteChange = static_cast<std::uint32_t>(_noteChangeIntervalS * jitter * _sampleRate);
            }
            else
            {
                _samplesUntilNoteChange -= static_cast<std::uint32_t>(blockSamples);
            }
        }
        _currentFrequencyHz += noteGlideSmoothing * (_targetFrequencyHz - _currentFrequencyHz);

        if (_samplesUntilWobbleRetarget <= blockSamples)
        {
            const float randomUnit = _wobbleRng.nextFloat01() * 2.0F - 1.0F; // -1..1
            _wobbleTargetRatio = 1.0F + randomUnit * _driftDepth;
            const float jitter = 0.5F + _wobbleRng.nextFloat01();
            _samplesUntilWobbleRetarget = static_cast<std::uint32_t>(_wobbleRetargetS * jitter * _sampleRate);
        }
        else
        {
            _samplesUntilWobbleRetarget -= static_cast<std::uint32_t>(blockSamples);
        }
        _wobbleRatio += wobbleSmoothing * (_wobbleTargetRatio - _wobbleRatio);

        _osc.setFrequency(_currentFrequencyHz * _wobbleRatio * _detuneRatio);
    }

    [[gnu::always_inline]] float process()
    {
        return _osc.process();
    }

    // Diagnostic/test accessor -- the oscillator's current live frequency (post-glide, -wobble,
    // -detune), same shape as TwinPluck::voiceFrequencyHz().
    float frequencyHz() const
    {
        return _currentFrequencyHz * _wobbleRatio * _detuneRatio;
    }

private:
    static constexpr float maxDriftDepth{0.02F}; // +-2% at full drift depth
    static constexpr core::Range noteChangeSRange{6.0F, 45.0F};
    static constexpr core::Range wobbleRetargetSRange{0.15F, 1.0F};
    static constexpr float noteGlideSmoothing{0.01F}; // one-pole coefficient per block
    static constexpr float wobbleSmoothing{0.05F}; // one-pole coefficient per block

    core::WavetableOscillator<64U> _osc;
    core::Xorshift32 _noteRng{1U};
    core::Xorshift32 _wobbleRng{1U};
    float _sampleRate{1.0F};
    float _detuneRatio{1.0F};
    float _currentFrequencyHz{rootFrequencyHz};
    float _targetFrequencyHz{rootFrequencyHz};
    float _wobbleRatio{1.0F};
    float _wobbleTargetRatio{1.0F};
    std::uint32_t _samplesUntilNoteChange{0U};
    std::uint32_t _samplesUntilWobbleRetarget{0U};
    float _driftDepth{0.006F};
    bool _frozen{false};
    float _noteChangeIntervalS{15.0F};
    float _wobbleRetargetS{0.4F};
};

} // namespace galerna::effects
