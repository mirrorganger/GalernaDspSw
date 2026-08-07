#pragma once

#include "galerna/core/Range.hpp"
#include "galerna/core/StateVariableFilter.hpp"
#include "galerna/effects/WindChimeVoice.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// Generative ambient wind-chime synth: a bank of independently-scheduled WindChimeVoice strikers
// (see WindChimeVoice.hpp), summed and tone-shaped by a resonant lowpass ("timbre" for cutoff,
// "resonance" for the peak), same filter role as ThxDeepNote. There is no working line-in on this
// board, so processBlock() ignores whatever the AudioBuffer already contains and overwrites it
// with the generated signal on both channels.
class WindChimes
{
public:
    // WindChimeVoice is cheaper per-sample than ThxVoice (one oscillator vs. ThxVoice's main
    // oscillator plus its amortized LFO), so this can afford more voices than ThxDeepNote's
    // hardware-measured 7 -- 10 was tried first (a cost-based estimate, not DWT-profiled) and
    // confirmed on real hardware not to work, so this settled at 8 (ThxDeepNote's own
    // proven-safe ceiling) for a while. Lowered further, in two steps, once CloudReverb (see
    // docs/architecture.md's Reverb section) was chained after it: real hardware DWT profiling
    // showed the combined WindChimes+CloudReverb cost massively over budget (maxProcessCycles
    // ~5.8x the per-block budget even after force-inlining the hot path). 8->5 plus cutting
    // CloudReverb down to a single mono line got to ~1.2x over; 5->4 (plus trimming CloudReverb's
    // own diffuser/multitap further) got to ~1.1x; 4->3 finally landed under budget at 97.9% --
    // technically safe but matching a razor's edge this codebase has already judged too risky
    // (see ThxDeepNote::voiceCount's own 98.8%-was-too-risky comment), so CloudReverb's last
    // diffuser stage was also cut (see CloudReverb.hpp) rather than pushing voiceCount down
    // further and losing more chime density. Final verified state at voiceCount 3: 39,165/47,190
    // cycles (83.0% of budget), clipCount 764/~1.94M samples (0.04%), errorCount 0, 30s soak.
    // Check maxProcessCycles()/clipCount() over SWO (see applications/wind_chimes/app.cpp's
    // printPotValues()) before raising this again.
    static constexpr std::size_t voiceCount{3U};

    void init(float sampleRate)
    {
        for (std::size_t voice = 0U; voice < voiceCount; ++voice)
        {
            // Distinct, fixed nonzero seeds so each voice's strike schedule/pitch sequence is
            // decorrelated from the others but reproducible run to run.
            _voices[voice].init(sampleRate, seedTable[voice]);
        }
        _filter.init(sampleRate);
        setTimbre(1.0F);
        setResonance(0.0F);
        setDensity(0.5F);
        setSpread(0.5F);
        setDecay(0.5F);
    }

    // density: 0..1, how often voices strike (0 = rare, sparse chimes; 1 = frequent, busy).
    void setDensity(float density)
    {
        _density = std::clamp(density, 0.0F, 1.0F);
    }

    // spread: 0..1, how many octaves above the scale root a strike can land on.
    void setSpread(float spread)
    {
        _spread = std::clamp(spread, 0.0F, 1.0F);
    }

    // decay: 0..1, exponentially maps to how long (in seconds) a struck note takes to ring out,
    // between minDecayS (short, plucky) and maxDecayS (long, sustained bell tails).
    void setDecay(float decay)
    {
        const float normalized = std::clamp(decay, 0.0F, 1.0F);
        _ringDurationS = decaySRange.exponential(normalized);
    }

    // timbre: 0..1, exponentially maps to the filter cutoff between minCutoffHz and maxCutoffHz
    // (0 = darkest, 1 = brightest).
    void setTimbre(float timbre)
    {
        const float normalized = std::clamp(timbre, 0.0F, 1.0F);
        _filter.setCutoff(cutoffHzRange.exponential(normalized));
    }

    // resonance: 0..1, filter peak at the cutoff frequency. Output is compensated (halved at
    // resonance 1) to keep the peak from pushing back into int16 clipping, same reasoning as
    // ThxDeepNote::setResonance().
    void setResonance(float resonance)
    {
        const float normalized = std::clamp(resonance, 0.0F, 1.0F);
        _filter.setResonance(normalized);
        _resonanceOutputCompensation = 1.0F - normalized * 0.5F;
    }

    // How many of the voiceCount voices are actually summed; 0 produces silence.
    void setActiveVoiceCount(std::size_t count)
    {
        _activeVoiceCount = std::min(count, voiceCount);
    }

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

        for (std::size_t voice = 0U; voice < _activeVoiceCount; ++voice)
        {
            _voices[voice].update(_density, _spread, _ringDurationS);
        }

        const float scale = headroom / static_cast<float>(_activeVoiceCount);
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
    static constexpr float headroom{2.0F};
    static constexpr core::Range cutoffHzRange{150.0F, 5'000.0F};
    static constexpr core::Range decaySRange{0.2F, 3.0F};
    static constexpr std::array<std::uint32_t, voiceCount> seedTable{
        0x9E3779B9U, 0x85EBCA6BU, 0xC2B2AE35U};

    std::array<WindChimeVoice, voiceCount> _voices{};
    std::size_t _activeVoiceCount{voiceCount};
    float _density{0.5F};
    float _spread{0.5F};
    float _ringDurationS{1.4F};
    core::StateVariableFilter _filter;
    float _resonanceOutputCompensation{1.0F};
};

} // namespace galerna::effects
