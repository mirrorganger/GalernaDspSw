#pragma once

#include "galerna/core/Range.hpp"
#include "galerna/core/StateVariableFilter.hpp"
#include "galerna/core/WavetableOscillator.hpp"
#include "galerna/core/Xorshift32.hpp"
#include "galerna/effects/Grain.hpp"
#include "galerna/effects/GrainBuffer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// A granular-synthesis texture generator: a fixed-pitch WavetableOscillator drone continuously
// feeds a shared GrainBuffer (see GrainBuffer.hpp), and a pool of Grain "voices" (see Grain.hpp)
// scatter enveloped fragments of that buffer's recent history back out, each at its own
// randomized start position ("spray") and playback rate ("pitch spread"), scheduled at a
// jittered interval the same way WindChimeVoice schedules its strikes. There is no working
// line-in on this board (see docs/progress.md), so -- like WindChimes/TwinPluck/AmbientPad --
// this generates its own source material rather than granulating a live signal; setFrozen()
// stops writing new material into the buffer so the grain pool keeps granulating a fixed
// snapshot instead (the genre-standard "freeze" control, e.g. Mutable Instruments Clouds).
//
// Summed and tone-shaped by a shared resonant lowpass ("timbre" for cutoff, "resonance" for the
// peak), same filter role as every other generative app here. processBlock() overwrites the
// AudioBuffer with the generated signal on both channels (mono-out, matching every other app).
class GranularCloud
{
public:
    // Hardware-verified (see this header's CPU budget note): grainPoolSize 6 measured
    // 53,198/47,190 cycles (112.7% of budget) -- over, unlike the cost-model guess this started
    // from (a Grain::process() call costs more than a single ModulatedDelayLine tap read, since
    // every pool slot -- active or not -- is looped over and called every sample, same
    // always-loop-every-slot shape as TwinPluck's unconditional voice sum). 4 still measured over
    // (48,721/47,190, 103.2%, even after fixing scheduleNextGrain() to drop its std::pow() call
    // -- see that method's comment). Final verified state at grainPoolSize 2: 38,769/47,190
    // cycles (82.15% of budget), clipCount 0/~1.51M samples, errorCount 0, 30s soak -- comfortably
    // clear of the >90%-is-too-risky precedent (see CloudReverb.hpp's rejected-97.9%-reading
    // comment), in the same margin range as WindChimes/AmbientPad's own final figures. From the
    // pool-4/pool-2 data points, each grain slot costs ~4,976 cycles/block and the fixed
    // (GranularCloud-overhead + chained CloudReverb) cost alone is ~28,800 (61% of budget) --
    // pool 3 would land at ~92.7%, matching this codebase's own precedent for "too tight, no
    // margin for jitter", so this stayed at 2 rather than splitting the difference. Two
    // simultaneous grains is sparser than a typical granular synth; if a denser cloud is wanted
    // later, dropping CloudReverb from this app's chain (not tuning it further -- it's already at
    // its minimum diffuser/line count) is the only lever with enough headroom to matter, and
    // would need its own re-verification.
    static constexpr std::size_t grainPoolSize{2U};
    // Fixed source pitch, not pot-controlled -- same idiom as AmbientPad: the
    // spray/density/pitch-spread controls ARE the instrument, no manual pitch pot needed.
    static constexpr float rootFrequencyHz{110.0F};

    void init(float sampleRate)
    {
        _sampleRate = sampleRate;
        _source.init(sampleRate);
        _source.setFrequency(rootFrequencyHz);
        _grainBuffer.init();
        _samplesUntilNextGrain = 0U;
        _filter.init(sampleRate);
        setGrainSize(0.3F);
        setDensity(0.5F);
        setSpray(0.3F);
        setPitchSpread(0.0F);
        setTimbre(0.6F);
        setResonance(0.0F);
        setFrozen(false);
    }

    // normalized: 0..1, exponentially maps to grain length between grainDurationSRange.min
    // (short, granular/glitchy) and .max (long, closer to a smeared drone).
    void setGrainSize(float normalized)
    {
        const float clamped = std::clamp(normalized, 0.0F, 1.0F);
        _grainDurationS = grainDurationSRange.exponential(clamped);
    }

    // normalized: 0..1, how often new grains are scheduled (0 = sparse, 1 = dense/busy).
    void setDensity(float normalized)
    {
        _density = std::clamp(normalized, 0.0F, 1.0F);
    }

    // normalized: 0..1, how widely each grain's start position scatters across the safe part of
    // GrainBuffer's history. 0 = every grain reads the same fixed lag (a static, repeating
    // texture); 1 = scattered across the whole safe range (a washy, cloud-like texture).
    void setSpray(float normalized)
    {
        _spray = std::clamp(normalized, 0.0F, 1.0F);
    }

    // normalized: 0..1, per-grain random playback-rate deviation around 1.0 (see
    // Grain::process()'s delay-modulation comment). 0 = every grain at unity rate (no pitch
    // shift); 1 = wide scatter, the classic granular "shimmer".
    void setPitchSpread(float normalized)
    {
        _pitchSpread = std::clamp(normalized, 0.0F, 1.0F);
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

    // frozen: true stops writing new source material into GrainBuffer -- existing and future
    // grains keep reading whatever snapshot was captured at the moment this was set, instead of
    // the live (still-advancing) drone.
    void setFrozen(bool frozen)
    {
        _frozen = frozen;
    }

    // Forces every pool slot to spawn a fresh grain right now, regardless of the scheduler's
    // current countdown -- a manual "stutter"/accent gesture.
    void retriggerAll()
    {
        for (auto& grain : _grains)
        {
            triggerGrain(grain);
        }
    }

    // Diagnostic/LED-mirroring accessor -- how many pool slots are currently sounding.
    [[nodiscard]] std::size_t activeGrainCount() const
    {
        std::size_t count{0U};
        for (const auto& grain : _grains)
        {
            if (grain.isActive())
            {
                ++count;
            }
        }
        return count;
    }

    // flatten: forces the entire per-sample call chain (all pool Grains + the shared filter) to
    // inline into this one function body -- see WavetableOscillator's always_inline comment for
    // why cross-function call overhead matters this much on this hardware.
    template <typename AudioBuffer>
    [[gnu::flatten]] void processBlock(AudioBuffer& buffer)
    {
        for (std::size_t frame = 0U; frame < buffer.size(); ++frame)
        {
            if (!_frozen)
            {
                _grainBuffer.write(_source.process());
            }

            // Per-sample, not block-rate: density is a grains-per-second rate, so the scheduler
            // needs sample-accurate timing the same way WindChimeVoice's strike countdown does --
            // a single decrement/compare per sample is negligible cost, already proven so by
            // WindChimes' 3 independent per-sample voice state machines fitting budget.
            if (_samplesUntilNextGrain == 0U)
            {
                scheduleNextGrain();
                Grain* freeSlot = findFreeSlot();
                if (freeSlot != nullptr)
                {
                    triggerGrain(*freeSlot);
                }
            }
            else
            {
                --_samplesUntilNextGrain;
            }

            float sum = 0.0F;
            for (auto& grain : _grains)
            {
                sum += grain.process(_grainBuffer);
            }
            sum *= headroom / static_cast<float>(grainPoolSize);

            const float filtered = _filter.process(sum) * _resonanceOutputCompensation;
            buffer.left[frame] = filtered;
            buffer.right[frame] = filtered;
        }
    }

private:
    static constexpr std::size_t bufferSize{8'192U}; // ~256 ms at ~32 kHz, see class README entry
    // +-50% playback rate at full pitch spread -- also used to size the safe delay-scatter range
    // below, since Grain::process()'s delay-modulation drifts the read position by up to this
    // fraction of a grain's duration over its lifetime.
    static constexpr float maxPitchDeviation{0.5F};
    // Hardware-verified (see this header's CPU budget note): 3.0 was tuned for grainPoolSize 6
    // and left unchanged as grainPoolSize was cut for CPU reasons, which silently tripled this
    // stage's gain (headroom/grainPoolSize) along the way -- measured clipCount 223,748/~1.58M
    // samples (14.2%) at grainPoolSize 2. Retuned down to grainPoolSize 2's own working point.
    static constexpr float headroom{1.0F};
    static constexpr core::Range grainDurationSRange{0.01F, 0.15F}; // 10 ms .. 150 ms
    static constexpr core::Range grainIntervalSRange{0.025F, 0.5F}; // 40 .. 2 grains/sec
    static constexpr core::Range cutoffHzRange{150.0F, 5'000.0F};

    // Higher density -> shorter mean wait; jittered so pool slots don't lock into a mechanical,
    // repeating cadence -- same shape as WindChimeVoice::scheduleNextStrike(), including its
    // choice of .linear() over .exponential() here specifically: this runs on the audio-rate
    // hot path (called from processBlock() every time a grain triggers, unlike setGrainSize()'s
    // own .exponential() call, which only ever runs from the control-rate tick() loop), and
    // .exponential()'s std::pow() call measured as a real, hardware-verified contributor to
    // maxProcessCycles here -- see this header's CPU budget note.
    void scheduleNextGrain()
    {
        const float meanIntervalS = grainIntervalSRange.linear(1.0F - _density);
        const float jitter = 0.5F + _triggerRng.nextFloat01();
        _samplesUntilNextGrain = static_cast<std::uint32_t>(meanIntervalS * jitter * _sampleRate);
    }

    Grain* findFreeSlot()
    {
        for (auto& grain : _grains)
        {
            if (!grain.isActive())
            {
                return &grain;
            }
        }
        return nullptr; // pool exhausted -- next scheduled grain will find a slot once one frees
    }

    // Picks a randomized start position (within a range that leaves enough margin on both sides
    // for the widest possible pitch-spread drift over the grain's full duration -- see
    // maxPitchDeviation's comment) and playback rate, then triggers the given pool slot.
    void triggerGrain(Grain& grain)
    {
        const auto durationSamples
            = static_cast<std::uint32_t>(std::max(1.0F, _grainDurationS * _sampleRate));

        const float maxDriftSamples = maxPitchDeviation * static_cast<float>(durationSamples);
        const float safeMin = maxDriftSamples;
        const float safeMax = static_cast<float>(bufferSize) - maxDriftSamples;
        const float sprayRange = std::max(0.0F, safeMax - safeMin);
        const float startDelaySamples = safeMin + _triggerRng.nextFloat01() * sprayRange * _spray;

        const float randomUnit = _triggerRng.nextFloat01() * 2.0F - 1.0F; // -1..1
        const float playbackRate = 1.0F + randomUnit * maxPitchDeviation * _pitchSpread;

        grain.trigger(startDelaySamples, playbackRate, durationSamples);
    }

    core::WavetableOscillator<64U> _source;
    GrainBuffer<bufferSize> _grainBuffer;
    std::array<Grain, grainPoolSize> _grains{};
    core::Xorshift32 _triggerRng{0x9E3779B9U};
    core::StateVariableFilter _filter;
    float _sampleRate{1.0F};
    float _grainDurationS{0.05F};
    float _density{0.5F};
    float _spray{0.3F};
    float _pitchSpread{0.0F};
    float _resonanceOutputCompensation{1.0F};
    std::uint32_t _samplesUntilNextGrain{0U};
    bool _frozen{false};
};

} // namespace galerna::effects
