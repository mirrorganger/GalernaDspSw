#pragma once

#include "galerna/core/Range.hpp"
#include "galerna/core/StateVariableFilter.hpp"
#include "galerna/effects/DriftVoice.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// A generative ambient drone pad: voiceCount DriftVoice oscillators, each self-drifting in pitch
// (see DriftVoice.hpp), summed and tone-shaped by a shared resonant lowpass ("timbre" for cutoff,
// "resonance" for the peak), same filter role as WindChimes/TwinPluck. Unlike TwinPluck's
// always-summed voices, only the first activeVoiceCount voices are summed each block -- same
// "active voice count" idea as WindChimes, since there's no gate to silence an unwanted voice here
// (every DriftVoice always produces sound once init()'d). There is no working line-in on this
// board, so processBlock() ignores whatever the AudioBuffer already contains and overwrites it
// with the generated signal on both channels.
//
// Detune/unison thickness comes from a fixed per-voice frequency ratio (see setDetune()), spread
// evenly around 1.0 across the active voices, rather than doubling each voice's oscillator count
// -- half the CPU cost of a classic 2-oscillator-per-voice unison stack for a similar beating
// effect, which matters on a board whose CPU budget is already this tight (see this class's
// voiceCount comment below).
class AmbientPad
{
public:
    // Starts at WindChimes' hardware-proven-safe voice count (83.0% of the per-block budget
    // chained into CloudReverb) rather than TwinPluck's 4, since each DriftVoice::process() is
    // PluckVoice::process()-shaped (one oscillator plus arithmetic, no envelope branching) --
    // comparable per-voice cost.
    //
    // CPU cost -- hardware-verified: chained after CloudReverb (see applications/ambient_drift/
    // app.cpp, same ProcessorChain shape as wind_chimes/twin_pluck) and measured via the same
    // DWT-cycle-counter method used to tune those: flashed to the real board, GDB-attached after a
    // ~2-minute soak, read Stm32I2sDuplexAudio::_maxProcessCycles/_errorCount and
    // DuplexAudioBlockProcessor::_clipCount directly.
    //   3 voices: maxProcessCycles 31,929/47,190 (67.66% of budget), clipCount 742/~4.00M samples
    //   (0.019%), errorCount 0.
    //   4 voices: maxProcessCycles 34,790/47,190 (73.72% of budget), clipCount 2/~5.00M samples
    //   (<0.001%), errorCount 0.
    // Both comfortably clear of the >90%-is-too-risky precedent (see CloudReverb.hpp's
    // rejected-97.9%-reading comment), with 4 voices still under WindChimes/TwinPluck's own ~83%
    // final figures. Re-verify the same way before raising this past 4.
    static constexpr std::size_t voiceCount{4U};

    void init(float sampleRate)
    {
        for (std::size_t voice = 0U; voice < voiceCount; ++voice)
        {
            _voices[voice].init(sampleRate, noteSeedTable[voice], wobbleSeedTable[voice]);
        }
        _filter.init(sampleRate);
        setActiveVoiceCount(voiceCount);
        setDetune(0.3F);
        setDriftDepth(0.3F);
        setEvolveRate(0.5F);
        setTimbre(0.6F);
        setResonance(0.0F);
    }

    // count: 0..voiceCount, how many voices are summed (0 produces silence). Same shape as
    // WindChimes::setActiveVoiceCount(); re-spreads detune across just the active voices.
    void setActiveVoiceCount(std::size_t count)
    {
        _activeVoiceCount = std::min(count, voiceCount);
        applyDetune();
    }

    // normalized: 0..1 -> 0..maxDetuneCents spread across the active voices (0 = unison, 1 = wide
    // chorus-y detune).
    void setDetune(float normalized)
    {
        _detuneNormalized = std::clamp(normalized, 0.0F, 1.0F);
        applyDetune();
    }

    void setDriftDepth(float normalized)
    {
        for (auto& voice : _voices)
        {
            voice.setDriftDepth(normalized);
        }
    }

    void setEvolveRate(float normalized)
    {
        for (auto& voice : _voices)
        {
            voice.setEvolveRate(normalized);
        }
    }

    // frozen: true holds the current chord (stops picking new pentatonic targets) on every voice,
    // while each voice's wobble keeps running -- see DriftVoice::setFrozen().
    void setFrozen(bool frozen)
    {
        for (auto& voice : _voices)
        {
            voice.setFrozen(frozen);
        }
    }

    // Forces every voice to immediately pick a new pentatonic target -- a manual "next chord"
    // trigger (see DriftVoice::reseed()).
    void reseed()
    {
        for (auto& voice : _voices)
        {
            voice.reseed();
        }
    }

    // timbre: 0..1, exponentially maps to the filter cutoff between minCutoffHz and maxCutoffHz
    // (0 = darkest, 1 = brightest). Same shape as WindChimes::setTimbre().
    void setTimbre(float timbre)
    {
        const float normalized = std::clamp(timbre, 0.0F, 1.0F);
        _filter.setCutoff(cutoffHzRange.exponential(normalized));
    }

    // resonance: 0..1, filter peak at the cutoff frequency. Output is compensated (halved at
    // resonance 1) to keep the peak from pushing back into int16 clipping, same reasoning as
    // WindChimes::setResonance().
    void setResonance(float resonance)
    {
        const float normalized = std::clamp(resonance, 0.0F, 1.0F);
        _filter.setResonance(normalized);
        _resonanceOutputCompensation = 1.0F - normalized * 0.5F;
    }

    // Diagnostic/test accessor -- voice's current live frequency, same shape as
    // TwinPluck::voiceFrequencyHz(). voice: caller-guaranteed in [0, voiceCount).
    float voiceFrequencyHz(std::size_t voice) const
    {
        return _voices[voice].frequencyHz();
    }

    // flatten: forces the entire per-sample call chain (all active DriftVoices + the shared
    // filter) to inline into this one function body -- see WavetableOscillator's always_inline
    // comment for why cross-function call overhead matters this much on this hardware.
    template <typename AudioBuffer>
    [[gnu::flatten]] void processBlock(AudioBuffer& buffer)
    {
        if (_activeVoiceCount == 0U)
        {
            for (std::size_t frame = 0U; frame < buffer.size(); ++frame)
            {
                buffer.left[frame] = 0.0F;
                buffer.right[frame] = 0.0F;
            }
            return;
        }

        // Block-rate (once per call), not per-sample -- see DriftVoice's class comment.
        for (std::size_t voice = 0U; voice < _activeVoiceCount; ++voice)
        {
            _voices[voice].updateDrift(buffer.size());
        }

        const float scale = 1.0F / static_cast<float>(_activeVoiceCount);
        for (std::size_t frame = 0U; frame < buffer.size(); ++frame)
        {
            float sum = 0.0F;
            for (std::size_t voice = 0U; voice < _activeVoiceCount; ++voice)
            {
                sum += _voices[voice].process();
            }
            sum *= scale;

            const float filtered = _filter.process(sum) * _resonanceOutputCompensation;

            buffer.left[frame] = filtered;
            buffer.right[frame] = filtered;
        }
    }

private:
    static constexpr float maxDetuneCents{35.0F};
    static constexpr core::Range cutoffHzRange{150.0F, 5'000.0F};
    static constexpr std::array<std::uint32_t, voiceCount> noteSeedTable{
        0x9E3779B9U, 0x85EBCA6BU, 0xC2B2AE35U, 0x27D4EB2FU};
    static constexpr std::array<std::uint32_t, voiceCount> wobbleSeedTable{
        0xA53F1E7BU, 0x2E8F9C41U, 0x6D4C1F0BU, 0xB55A4F09U};

    // Spreads each active voice's fixed detune ratio evenly around 1.0 (voice i of n gets
    // (i - (n-1)/2) * spreadCents/(n-1)), scaled by _detuneNormalized -- re-applied whenever
    // detune or active voice count changes, see setDetune()/setActiveVoiceCount().
    void applyDetune()
    {
        if (_activeVoiceCount == 0U)
        {
            return;
        }
        if (_activeVoiceCount == 1U)
        {
            _voices[0].setDetuneRatio(1.0F);
            return;
        }
        const float spreadCents = maxDetuneCents * _detuneNormalized;
        const float n = static_cast<float>(_activeVoiceCount - 1U);
        for (std::size_t voice = 0U; voice < _activeVoiceCount; ++voice)
        {
            const float cents = (static_cast<float>(voice) - n * 0.5F) * spreadCents / n;
            _voices[voice].setDetuneRatio(std::pow(2.0F, cents / 1200.0F));
        }
    }

    std::array<DriftVoice, voiceCount> _voices{};
    std::size_t _activeVoiceCount{voiceCount};
    float _detuneNormalized{0.0F};
    core::StateVariableFilter _filter;
    float _resonanceOutputCompensation{1.0F};
};

} // namespace galerna::effects
