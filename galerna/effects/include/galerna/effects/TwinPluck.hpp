#pragma once

#include "galerna/core/PentatonicScale.hpp"
#include "galerna/core/Range.hpp"
#include "galerna/core/StateVariableFilter.hpp"
#include "galerna/core/Xorshift32.hpp"
#include "galerna/effects/PluckVoice.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// A 4-voice gated instrument: voices 0/1 (button1Voice/button2Voice) are independently pitched
// via a pot and gated by the two push buttons; voices 2/3 (droneRootVoice/droneFifthVoice) have a
// fixed pitch (the scale root, and a perfect fifth above it) and are gated by the two on/off
// switches instead -- a switch's steady position maps naturally onto PluckVoice's held/released
// gate the same way a button's press/release does, so both voice pairs share the exact same
// PluckVoice (see its own doc comment) and gating API; only how each voice's gate signal and
// pitch are sourced differs, all decided by TwinPluckApp. All 4 voices are summed and tone-shaped
// by a single shared resonant lowpass ("timbre" for cutoff, "resonance" for the peak), same filter
// role as WindChimes/ThxDeepNote. There is no "active voice count" -- all 4 voices are always
// summed, a silent (never gated on, or fully released) voice just contributes 0. There is no
// working line-in on this board, so processBlock() ignores whatever the AudioBuffer already
// contains and overwrites it with the generated signal on both channels.
//
// The two drone voices additionally get a semi-random pitch wander (see DroneWander/updateWander()
// below) so a held drone drifts organically around its center frequency instead of holding a
// perfectly static tone -- the button voices don't get this, since their pitch is already under
// live pot control.
//
// CPU cost -- hardware-verified: chained after CloudReverb (see applications/twin_pluck/app.cpp,
// same ProcessorChain shape as wind_chimes) and measured via the same DWT-cycle-counter method
// used to tune WindChimes+CloudReverb (see WindChimes::voiceCount's comment): flashed to the real
// board, GDB-attached after a 30s soak, read Stm32I2sDuplexAudio::_maxProcessCycles/_errorCount
// and DuplexAudioBlockProcessor::_clipCount directly. Going from 2 to 4 voices (adding the two
// switch-gated drones) raised maxProcessCycles from 30,525 to 38,829 -- roughly +8,300 cycles for
// 2 more voices, consistent with each PluckVoice::process() costing about the same regardless of
// which gate drives it. Adding the semi-random drone pitch wander on top (see this class's own
// comment above) moved maxProcessCycles by only +34 cycles (38,829 -> 38,863) -- confirms updating
// it at block-rate inside processBlock(), instead of per-sample, keeps this feature's cost
// negligible. Latest result: maxProcessCycles 38,863/47,190 (82.35% of budget), clipCount
// 0/22,890 calls, errorCount 0 -- real margin, comparable to WindChimes' own 83.0% final figure
// and clear of the >90%-is-too-risky precedent (see ThxDeepNote::voiceCount's comment). Re-verify
// the same way before adding a 5th voice or anything else to this chain.
class TwinPluck
{
public:
    static constexpr std::size_t voiceCount{4U};

    // Named voice indices -- avoids magic numbers at both TwinPluck's and TwinPluckApp's call
    // sites, and documents which voice is which (2 pot-pitched/button-gated pluck voices, 2
    // fixed-pitch/switch-gated drone voices).
    static constexpr std::size_t button1Voice{0U};
    static constexpr std::size_t button2Voice{1U};
    static constexpr std::size_t droneRootVoice{2U};
    static constexpr std::size_t droneFifthVoice{3U};

    // core::PentatonicScale::ratios index for a perfect fifth (3/2).
    static constexpr std::size_t droneFifthScaleDegree{3U};

    void init(float sampleRate)
    {
        _sampleRate = sampleRate;
        for (auto& voice : _voices)
        {
            voice.init(sampleRate);
        }
        // Drone voices have no pot controlling their pitch (see this class's doc comment) --
        // fixed once here via setPendingFrequencyHz(), which bypasses PluckVoice's normalized-pot
        // scale quantization. They stay silent (State::idle) until their switch gates them on.
        _voices[droneRootVoice].setPendingFrequencyHz(PluckVoice::rootFrequencyHz);
        _voices[droneFifthVoice].setPendingFrequencyHz(
            PluckVoice::rootFrequencyHz * core::PentatonicScale::ratios[droneFifthScaleDegree]);
        // Distinct, fixed nonzero seeds (same reasoning as WindChimes' seedTable) so the two
        // drones' wander is decorrelated but reproducible run to run.
        _rootWander.rng = core::Xorshift32{0xA53F1E7BU};
        _fifthWander.rng = core::Xorshift32{0x2E8F9C41U};
        _rootWander.samplesUntilRetarget = 0U;
        _fifthWander.samplesUntilRetarget = 0U;
        _filter.init(sampleRate);
        setTimbre(1.0F);
        setResonance(0.0F);
        setDecay(0.5F);
    }

    // voice: caller-guaranteed in [0, voiceCount) -- only ever called by TwinPluckApp with
    // button1Voice/button2Voice (the drone voices' pitch is fixed, see init()), so this
    // deliberately has no runtime bounds check.
    void setVoicePitch(std::size_t voice, float normalized)
    {
        _voices[voice].setPendingFrequency(normalized);
    }

    // Gates voice on: sustains at full amplitude for as long as the caller doesn't call
    // noteOff(voice) (TwinPluckApp calls this on a button press edge or a switch's on edge).
    void noteOn(std::size_t voice)
    {
        _voices[voice].noteOn();
    }

    // Gates voice off: starts its release using the current decay/release setting (TwinPluckApp
    // calls this on a button release edge or a switch's off edge). Using _ringDurationS at release
    // time (rather than capturing it at noteOn time) means adjusting the Decay pot while a note is
    // held changes how long *that* note releases, not just future ones.
    void noteOff(std::size_t voice)
    {
        _voices[voice].noteOff(_ringDurationS);
    }

    bool isVoiceRinging(std::size_t voice) const
    {
        return _voices[voice].isRinging();
    }

    // Diagnostic/test accessor -- frequency of the most recent noteOn() for this voice (see
    // PluckVoice::frequencyHz()).
    float voiceFrequencyHz(std::size_t voice) const
    {
        return _voices[voice].frequencyHz();
    }

    // decay: 0..1, exponentially maps to how long (in seconds) a voice takes to ring out after
    // its button is released, between minDecayS (short) and maxDecayS (long, sustained tail).
    // Same shape as WindChimes::setDecay(); has no effect while a voice is held (see noteOff()).
    void setDecay(float decay)
    {
        const float normalized = std::clamp(decay, 0.0F, 1.0F);
        _ringDurationS = decaySRange.exponential(normalized);
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
    // WindChimes::setResonance()/ThxDeepNote::setResonance().
    void setResonance(float resonance)
    {
        const float normalized = std::clamp(resonance, 0.0F, 1.0F);
        _filter.setResonance(normalized);
        _resonanceOutputCompensation = 1.0F - normalized * 0.5F;
    }

    // flatten: forces the entire per-sample call chain (all 4 PluckVoices + the shared filter) to
    // inline into this one function body -- see WavetableOscillator's always_inline comment for
    // why cross-function call overhead matters this much on this hardware.
    template <typename AudioBuffer>
    [[gnu::flatten]] void processBlock(AudioBuffer& buffer)
    {
        // Block-rate (once per call), not per-sample -- a drift this slow doesn't need audio-rate
        // precision, so updating it here instead of inside the per-frame loop keeps the feature's
        // CPU cost negligible against the per-block budget.
        updateWander(_rootWander, droneRootVoice, PluckVoice::rootFrequencyHz, buffer.size());
        updateWander(
            _fifthWander,
            droneFifthVoice,
            PluckVoice::rootFrequencyHz * core::PentatonicScale::ratios[droneFifthScaleDegree],
            buffer.size());

        for (std::size_t frame = 0U; frame < buffer.size(); ++frame)
        {
            float sum = 0.0F;
            for (auto& voice : _voices)
            {
                sum += voice.process();
            }
            sum *= headroom;

            const float filtered = _filter.process(sum) * _resonanceOutputCompensation;

            buffer.left[frame] = filtered;
            buffer.right[frame] = filtered;
        }
    }

private:
    // Fixed scale (all 4 voices always summed, no active-count concept) -- deliberately not
    // computed from an "active voice count" the way WindChimes::headroom's scale is, since
    // TwinPluck has no such notion.
    static constexpr float headroom{1.0F / static_cast<float>(voiceCount)};
    static constexpr core::Range cutoffHzRange{150.0F, 5'000.0F};
    static constexpr core::Range decaySRange{0.2F, 3.0F};

    // Semi-random pitch wander for the drone voices: periodically (every ~wanderRetargetS
    // seconds on average, jittered so root/fifth don't lock into the same rhythm) picks a new
    // random target frequency ratio within +-wanderDepth of 1.0, then a one-pole filter
    // (wanderSmoothing, applied once per processBlock() call) chases that target so the pitch
    // glides smoothly rather than jumping -- an organic drift around each drone's center pitch,
    // reminiscent of analog oscillator drift, rather than a perfectly static drone or a
    // perfectly periodic LFO wobble.
    static constexpr float wanderDepth{0.01F}; // +-1% frequency deviation (~+-17 cents)
    static constexpr float wanderSmoothing{0.05F}; // one-pole coefficient per block
    static constexpr float wanderRetargetS{.2F}; // average seconds between new targets

    struct DroneWander
    {
        // Placeholder seed -- init() reassigns a real one per instance, same
        // construct-then-reseed pattern WindChimeVoice::init() uses for its own _rng.
        core::Xorshift32 rng{1U};
        float currentRatio{1.0F};
        float targetRatio{1.0F};
        std::uint32_t samplesUntilRetarget{0U};
    };

    void updateWander(DroneWander& wander, std::size_t voice, float centerFrequencyHz, std::size_t blockSamples)
    {
        if (wander.samplesUntilRetarget <= blockSamples)
        {
            const float randomUnit = wander.rng.nextFloat01() * 2.0F - 1.0F; // -1..1
            wander.targetRatio = 1.0F + randomUnit * wanderDepth;
            const float jitter = 0.5F + wander.rng.nextFloat01(); // 0.5x-1.5x, avoids a locked cadence
            wander.samplesUntilRetarget = static_cast<std::uint32_t>(wanderRetargetS * jitter * _sampleRate);
        }
        else
        {
            wander.samplesUntilRetarget -= static_cast<std::uint32_t>(blockSamples);
        }
        wander.currentRatio += wanderSmoothing * (wander.targetRatio - wander.currentRatio);
        _voices[voice].modulateFrequencyHz(centerFrequencyHz * wander.currentRatio);
    }

    std::array<PluckVoice, voiceCount> _voices{};
    core::StateVariableFilter _filter;
    float _ringDurationS{1.4F};
    float _resonanceOutputCompensation{1.0F};
    float _sampleRate{1.0F};
    DroneWander _rootWander{};
    DroneWander _fifthWander{};
};

} // namespace galerna::effects
