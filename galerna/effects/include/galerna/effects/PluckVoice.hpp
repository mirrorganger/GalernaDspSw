#pragma once

#include "galerna/core/PentatonicScale.hpp"
#include "galerna/core/WavetableOscillator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace galerna::effects
{

// One voice of a gated instrument: silent until externally noteOn()'d, then sustains at full
// amplitude for as long as it's held (no self-decay while held), and only starts ringing out
// with an exponential decay -- same math as WindChimeVoice's strike() -- once noteOff() is
// called. This never self-schedules anything (no RNG, no internal timing) -- the caller
// (TwinPluck, driven by a physical button's press/release edges) decides when to gate on/off,
// so this class only owns pitch quantization and the held/release envelope.
class PluckVoice
{
public:
    // Same low end as WindChimeVoice's chime register -- there's no reason to pick a different
    // tessitura just because this instrument is gated rather than struck-and-decorrelated.
    static constexpr float rootFrequencyHz{220.0F};

    // How many octaves above rootFrequencyHz a pitch pot's full travel spans.
    static constexpr std::size_t octaveRange{3U};

    void init(float sampleRate)
    {
        _sampleRate = sampleRate;
        _osc.init(sampleRate);
        _pendingFrequencyHz = rootFrequencyHz;
        _amplitude = 0.0F;
        _decayPerSample = 1.0F;
        _state = State::idle;
    }

    // normalized: 0..1 pot value, quantized to one of
    // core::PentatonicScale::degreeCount*octaveRange notes. Always stored as the pending
    // frequency for the *next* noteOn(); additionally applied immediately
    // (via modulateFrequencyHz()) if the voice is currently ringing (held or releasing), so
    // turning the pitch knob while a button is held retunes the note in real time -- since pitch
    // is scale-quantized rather than continuous, this is a discrete jump between notes, not a
    // glissando/glide.
    void setPendingFrequency(float normalized)
    {
        constexpr std::size_t stepCount{core::PentatonicScale::degreeCount * octaveRange};
        const auto totalStep = static_cast<std::size_t>(
            std::clamp(normalized, 0.0F, 1.0F) * static_cast<float>(stepCount));
        const std::size_t step = std::min(totalStep, stepCount - 1U);
        const std::size_t degree = step % core::PentatonicScale::degreeCount;
        const auto octave = static_cast<float>(step / core::PentatonicScale::degreeCount);
        setPendingFrequencyHz(rootFrequencyHz * core::PentatonicScale::ratios[degree] * std::pow(2.0F, octave));
    }

    // Sets the pending frequency directly, bypassing scale quantization -- used for voices with a
    // fixed pitch that isn't pot-controlled (see TwinPluck's switch-gated drone voices, which
    // never call this again after init() so the live-retune branch below never triggers for
    // them). Same "applies immediately if ringing, otherwise queued for the next noteOn()"
    // semantics as setPendingFrequency(normalized) -- see its comment.
    void setPendingFrequencyHz(float frequencyHz)
    {
        _pendingFrequencyHz = frequencyHz;
        if (isRinging())
        {
            modulateFrequencyHz(frequencyHz);
        }
    }

    // Applies the pending frequency and gates the voice on: amplitude jumps to full and stays
    // there (no attack ramp, same instant-on simplicity as the envelope this replaced) for as
    // long as the voice stays in State::held -- i.e. until noteOff() is called. Safe to call at
    // any time (e.g. re-pressed mid-release): always restarts cleanly at full amplitude.
    void noteOn()
    {
        _osc.setFrequency(_pendingFrequencyHz);
        _frequencyHz = _pendingFrequencyHz;
        _amplitude = 1.0F;
        _state = State::held;
    }

    // Starts the release: decayPerSample is precomputed so amplitude reaches silenceThreshold
    // after releaseDurationS seconds -- identical math to WindChimeVoice::strike(), just applied
    // on release instead of immediately on trigger. No-op if the voice isn't currently held (e.g.
    // a stray release with nothing gated on).
    void noteOff(float releaseDurationS)
    {
        if (_state != State::held)
        {
            return;
        }
        const float releaseSamples = releaseDurationS * _sampleRate;
        _decayPerSample = std::pow(silenceThreshold, 1.0F / releaseSamples);
        _state = State::releasing;
    }

    bool isRinging() const
    {
        return _state != State::idle;
    }

    // Directly sets the oscillator's live frequency, bypassing the pending/deferred mechanism --
    // for continuously modulating an already-ringing voice (e.g. TwinPluck's wandering drone
    // pitch), as opposed to setPendingFrequency()/setPendingFrequencyHz()'s "takes effect at the
    // next noteOn()" semantics. Harmless to call while idle or releasing -- just moves the
    // (possibly inaudible or decaying) oscillator's frequency, no envelope/state side effects.
    void modulateFrequencyHz(float frequencyHz)
    {
        _osc.setFrequency(frequencyHz);
        _frequencyHz = frequencyHz;
    }

    // Frequency of the most recent noteOn() (held after the voice rings back to silence, until
    // the next noteOn() overwrites it) -- same accessor shape as WindChimeVoice::frequencyHz().
    float frequencyHz() const
    {
        return _frequencyHz;
    }

    [[gnu::always_inline]] float process()
    {
        if (_state == State::releasing)
        {
            _amplitude *= _decayPerSample;
            if (_amplitude < silenceThreshold)
            {
                _amplitude = 0.0F;
                _state = State::idle;
            }
        }

        return _osc.process() * _amplitude;
    }

private:
    enum class State
    {
        idle,
        held,
        releasing
    };

    static constexpr float silenceThreshold{0.001F};

    // Only member buffer is WavetableOscillator<64U>'s 256B table -- unlike CloudReverb, whose
    // multi-KB buffers risked landing in FLASH-duplicated .data via constant-initialization, this
    // class is too small for that to matter and doesn't need CloudReverb's empty-user-provided-
    // constructor workaround.
    core::WavetableOscillator<64U> _osc;
    float _sampleRate{1.0F};
    float _pendingFrequencyHz{rootFrequencyHz};
    float _frequencyHz{rootFrequencyHz};
    float _amplitude{0.0F};
    float _decayPerSample{1.0F};
    State _state{State::idle};
};

} // namespace galerna::effects
