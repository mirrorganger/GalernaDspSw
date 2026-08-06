#pragma once

#include "galerna/effects/CloudReverbLine.hpp"
#include "galerna/effects/MultitapDelay.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace galerna::effects
{

// "CloudSeed-lite": a scaled-down homage to CloudSeed/CloudReverb's algorithmic-reverb topology
// (multitap early reflections, plus a late-reverb line with an LFO-modulated delay and diffuser
// in its feedback loop -- see CloudReverbLine.hpp), sized to fit this board's memory/CPU budget
// instead of CloudSeed's original desktop-VST scale (up to 50 early taps, 8 diffuser stages, 12
// late lines). See docs/architecture.md's Reverb section for the block diagram and the full
// sizing history -- this is CloudReverb's *second* size: the first attempt (2 independent
// per-channel tanks, 3 late lines each with their own diffuser, a 3-stage early diffuser chain)
// measured ~5.8x over the per-block CPU budget on real hardware even after force-inlining the hot
// path, so this cuts much harder than the original design intended: one shared mono tank instead
// of two per-channel ones (roughly halving cost outright, at the cost of the stereo-width trick
// the first attempt used), one late line instead of three, and no early-reflection diffuser stage
// at all. There is no working line-in on this board, so like WindChimes/ThxDeepNote this still
// expects whatever it's chained after (see ProcessorChain) to have already written a real signal
// into the AudioBuffer -- CloudReverb processes that signal in place rather than generating its
// own, and (since it's mono) reads only the left channel as its input, writing the identical wet
// signal to both output channels.
//
// CPU cost: verify via applications/wind_chimes/app.cpp's printPotValues() SWO diagnostics
// (audioEngine.maxProcessCycles() against its budget) before raising anything here again --
// WindChimes::voiceCount was also lowered (8 -> 5) alongside this cut, since even fully removing
// CloudReverb's stereo tanks and 2 of its 3 late lines wasn't expected to be enough on its own.
class CloudReverb
{
public:
    // Deliberately user-provided (even though empty): every member below is a literal-type
    // aggregate (floats, std::array<float, N>, no non-constexpr constructors anywhere in the
    // tree), so without this the compiler is allowed to "constant initialize" the whole object --
    // baking its default state (nearly all zero, but not quite: a handful of scalar fields, e.g.
    // the nested WavetableOscillators' default amplitude, are nonzero) into a real byte image
    // duplicated in FLASH and copied to RAM on every boot. A non-constexpr user-provided
    // constructor disqualifies that path, forcing dynamic initialization instead: a small
    // generated constructor function that only writes the actual nonzero scalar fields, so the
    // bulk buffers land in .bss (zero-filled, no FLASH image) the same way WindChimes does (see
    // docs/architecture.md's Reverb section for the measured RAM/FLASH difference this makes).
    CloudReverb() {}

    void init(float sampleRate)
    {
        _multitap.init(multitapSeed);
        _line.init(sampleRate, lineBaseDelayS, lineModDepthS, lineModRateHz);
        setMix(0.35F);
        setSize(0.5F);
    }

    // mix: 0..1, 0 = fully dry (bypass), 1 = fully wet.
    void setMix(float mix)
    {
        _wetMix = std::clamp(mix, 0.0F, 1.0F);
    }

    // size: 0..1, exponentially-flavored map (via minFeedback/maxFeedback) to the late line's
    // feedback gain -- higher sustains the recirculating tail longer, the same "how long does it
    // ring" idea as WindChimes::setDecay, just driven by feedback gain instead of an amplitude
    // envelope (see the RT60-vs-feedback discussion in docs/architecture.md's Reverb section).
    void setSize(float size)
    {
        const float normalized = std::clamp(size, 0.0F, 1.0F);
        _line.setFeedback(minFeedback + normalized * (maxFeedback - minFeedback));
    }

    // flatten: forces the entire per-sample call chain (MultitapDelay + the one CloudReverbLine)
    // to inline into this one function body -- see ModulatedDelayLine's always_inline comment for
    // why cross-function call overhead matters this much on this hardware.
    template <typename AudioBuffer>
    [[gnu::flatten]] void processBlock(AudioBuffer& buffer)
    {
        for (std::size_t frame = 0U; frame < buffer.size(); ++frame)
        {
            // Mono tank: WindChimes (the only thing chained before this today) already writes an
            // identical signal to both channels, so the left channel alone is read as the dry
            // input and the same wet signal is mixed into both outputs.
            const float dryLeft = buffer.left[frame];
            const float dryRight = buffer.right[frame];
            const float early = _multitap.process(dryLeft) * earlyGain;
            const float late = _line.process(dryLeft) * lateGain;
            const float wet = early + late;
            buffer.left[frame] = dryLeft * (1.0F - _wetMix) + wet * _wetMix;
            buffer.right[frame] = dryRight * (1.0F - _wetMix) + wet * _wetMix;
        }
    }

private:
    static constexpr std::size_t multitapBufferSize{1'536U}; // ~32ms@48kHz / ~47ms@32.55kHz
    static constexpr std::size_t multitapTapCount{2U};
    static constexpr std::uint32_t multitapSeed{0x2545F491U};

    static constexpr std::size_t lineDelayBufferSize{1'664U}; // 30ms line delay + 2ms mod headroom
    static constexpr std::size_t lineDiffuserBufferSize{576U}; // ~12ms@48kHz headroom
    static constexpr float lineBaseDelayS{0.030F};
    static constexpr float lineModDepthS{0.002F};
    static constexpr float lineModRateHz{0.09F};

    // earlyGain/lateGain: cost-based headroom estimates (like WindChimes::headroom), not yet
    // hardware-verified against DuplexAudioBlockProcessor::clipCount() -- see the class-level
    // comment above.
    static constexpr float earlyGain{0.35F};
    static constexpr float lateGain{0.5F};

    static constexpr float minFeedback{0.55F};
    static constexpr float maxFeedback{0.92F}; // kept in sync with CloudReverbLine::maxFeedback

    MultitapDelay<multitapBufferSize, multitapTapCount> _multitap;
    // 0 diffuser stages (down from CloudReverbLine's original 2, via 1 -- see this class's own
    // comment): the 1-stage version measured 46,225/47,190 cycles (97.9% of budget) on real
    // hardware -- technically under budget, but this codebase already has a precedent for not
    // trusting a measurement that tight (see ThxDeepNote::voiceCount's own comment: 98.8% was
    // judged "too risky... no margin left for jitter" and backed off further even though it
    // worked in that measurement). Dropping to 0 stages gives up the diffusion smearing --
    // CloudReverbLine degrades gracefully to a plain modulated, damped, fed-back delay -- in
    // exchange for real headroom instead of a razor's-edge budget fit. Verified on real hardware
    // at this final size (WindChimes::voiceCount 3, this at 0 diffuser stages, 2 multitap taps):
    // maxProcessCycles 39,165/47,190 (83.0% of budget), clipCount 764/~1.94M samples (0.04%),
    // errorCount 0, over a 30s soak.
    CloudReverbLine<lineDelayBufferSize, lineDiffuserBufferSize, 0U> _line;
    float _wetMix{0.35F};
};

} // namespace galerna::effects
