#pragma once

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
    // hardware-measured 7 -- but this count itself hasn't been DWT-profiled on real hardware
    // yet (see docs/progress.md / ThxDeepNote::voiceCount for how that measurement is done).
    // Check maxProcessCycles()/clipCount() over SWO (see applications/wind_chimes/app.cpp's
    // printPotValues()) after flashing and lower this if the budget percentage or clip count
    // climbs.
    static constexpr std::size_t voiceCount{10U};

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

    // timbre: 0..1, exponentially maps to the filter cutoff between minCutoffHz and maxCutoffHz
    // (0 = darkest, 1 = brightest).
    void setTimbre(float timbre)
    {
        const float normalized = std::clamp(timbre, 0.0F, 1.0F);
        const float cutoffHz = minCutoffHz * std::pow(maxCutoffHz / minCutoffHz, normalized);
        _filter.setCutoff(cutoffHz);
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
            _voices[voice].update(_density, _spread);
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
    static constexpr float minCutoffHz{150.0F};
    static constexpr float maxCutoffHz{5'000.0F};
    static constexpr std::array<std::uint32_t, voiceCount> seedTable{
        0x9E3779B9U,
        0x85EBCA6BU,
        0xC2B2AE35U,
        0x27D4EB2FU,
        0x165667B1U,
        0xD3A2646CU,
        0x6C62272EU,
        0x9AE16A3BU,
        0x1B873593U,
        0xE6546B64U};

    std::array<WindChimeVoice, voiceCount> _voices{};
    std::size_t _activeVoiceCount{voiceCount};
    float _density{0.5F};
    float _spread{0.5F};
    core::StateVariableFilter _filter;
    float _resonanceOutputCompensation{1.0F};
};

} // namespace galerna::effects
