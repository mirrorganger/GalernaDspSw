#pragma once

#include "Galerna/Core/StateVariableFilter.hpp"
#include "Galerna/Core/ThxVoice.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace galerna::effects
{

// Deep Note-style synth: a bank of ThxVoice oscillators glides from a scattered low cluster to
// their own target frequencies, gets summed and tone-shaped by a resonant lowpass ("timbre" for
// cutoff, "resonance" for the peak). There is no working line-in on this board, so
// processBlock() ignores whatever the AudioBuffer already contains and overwrites it with the
// generated signal on both channels.
class ThxDeepNote
{
public:
    // Real hardware measurement (24 MHz Cortex-M4, -Os) showed the audio ISR could only sustain
    // ~42% of real-time throughput at 15 voices -- see docs/progress.md. With the hot path forced
    // inline, 6/7/8 voices measured 72.1%/81.7%/92.1% of budget with the old one-pole filter. The
    // resonant StateVariableFilter costs more per sample: re-measured 8 voices with it in the
    // signal path at 98.8% (46662/47185) -- errorCount stayed 0 and it kept up in that specific
    // measurement, but with essentially no margin left for jitter, too risky to keep. Trying 7.
    static constexpr std::size_t voiceCount{7U};

    void init(float sampleRate)
    {
        float targetFrequencyHz = core::ThxVoice::lowestFrequencyHz;
        for (std::size_t voice = 0U; voice < voiceCount; ++voice)
        {
            _voices[voice].init(sampleRate, targetFrequencyHz, lfoRateTableHz[voice]);
            targetFrequencyHz *= 1.5F;
            if (targetFrequencyHz > core::ThxVoice::highestFrequencyHz)
            {
                targetFrequencyHz *= 0.5F;
            }
        }
        _filter.init(sampleRate);
        // Sensible defaults so the filter is already functional before the first setTimbre()/
        // setResonance() call arrives from the control-rate pot read (bright, no resonance --
        // matches the prior one-pole filter's default pass-through behavior).
        setTimbre(1.0F);
        setResonance(0.0F);
    }

    // pitch: 0..1, glides every active voice from the scattered cluster to its target chord.
    void setPitch(float pitch)
    {
        _pitch = pitch;
    }

    // pitchShift: 0..1, additively offsets the whole glide range by up to maxPitchShiftHz.
    void setPitchShift(float pitchShift)
    {
        _pitchShiftHz = std::clamp(pitchShift, 0.0F, 1.0F) * maxPitchShiftHz;
    }

    // timbre: 0..1, exponentially maps to the filter cutoff between minCutoffHz and
    // maxCutoffHz (0 = darkest, 1 = brightest).
    void setTimbre(float timbre)
    {
        const float normalized = std::clamp(timbre, 0.0F, 1.0F);
        const float cutoffHz = minCutoffHz * std::pow(maxCutoffHz / minCutoffHz, normalized);
        _filter.setCutoff(cutoffHz);
    }

    // resonance: 0..1, filter peak at the cutoff frequency (0 = clean, 1 = near
    // self-oscillation). A resonant peak is gain, so the output is compensated (halved at
    // resonance 1) to keep the peak from pushing back into int16 clipping -- see
    // docs/progress.md for why that matters here.
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

    // [[gnu::flatten]]: forces GCC to inline the entire per-sample call chain (ThxVoice::process
    // -> WavetableOscillator::process/interpolate) into this one function body, eliminating
    // cross-function call overhead that DWT profiling showed dominating the per-voice cost.
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
            _voices[voice].update(_pitch, _pitchShiftHz);
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
    // The reference's `3.0F / voicesToProcess` scale was tuned for DaisySP's oscillator
    // amplitude convention, not this project's -- real hardware confirmed (via
    // DuplexAudioBlockProcessor::clipCount()) that it hard-clips the int16 output hundreds of
    // times a second, audible as crackle. Lowered until clipCount stayed at 0.
    static constexpr float headroom{1.0F};
    static constexpr float maxPitchShiftHz{500.0F};
    static constexpr float minCutoffHz{150.0F};
    static constexpr float maxCutoffHz{5'000.0F};
    static constexpr std::array<float, voiceCount> lfoRateTableHz{
        5.0F, 9.17F, 13.33F, 17.5F, 21.67F, 25.83F, 30.0F};

    std::array<core::ThxVoice, voiceCount> _voices{};
    std::size_t _activeVoiceCount{voiceCount};
    float _pitch{0.0F};
    float _pitchShiftHz{0.0F};
    core::StateVariableFilter _filter;
    float _resonanceOutputCompensation{1.0F};
};

} // namespace galerna::effects
