#pragma once

#include "galerna/core/AudioBuffer.hpp"
#include "galerna/core/ProcessorChain.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace galerna::core
{

// Converts one ping-pong half of an interleaved stereo int16 DMA buffer to/from the float
// AudioBuffer domain and runs it through a ProcessorChain in place.
template <std::size_t MaxFramesPerHalf, typename... Processors>
class DuplexAudioBlockProcessor
{
public:
    explicit DuplexAudioBlockProcessor(ProcessorChain<Processors...>& chain)
        : _chain{chain}
    {
    }

    // rxInterleaved/txInterleaved: equal-length interleaved stereo int16 spans (L, R, L, R, ...),
    // one DMA ping-pong half. Frame count (size / 2) must not exceed MaxFramesPerHalf.
    void process(std::span<const std::int16_t> rxInterleaved, std::span<std::int16_t> txInterleaved)
    {
        const auto frames = rxInterleaved.size() / 2U;

        for (auto frame = std::size_t{}; frame < frames; ++frame)
        {
            _left[frame] = static_cast<float>(rxInterleaved[frame * 2U]) / sampleScale;
            _right[frame] = static_cast<float>(rxInterleaved[frame * 2U + 1U]) / sampleScale;
        }

        AudioBuffer buffer{
            std::span<float>{_left}.first(frames),
            std::span<float>{_right}.first(frames)};
        _chain.processBlock(buffer);

        for (auto frame = std::size_t{}; frame < frames; ++frame)
        {
            txInterleaved[frame * 2U] = toInt16(_left[frame]);
            txInterleaved[frame * 2U + 1U] = toInt16(_right[frame]);
        }
    }

    // Total samples that hit the int16 clamp below (i.e. the processed signal exceeded the
    // +-1.0 float range the DMA/codec domain expects). Diagnostic: hard clipping here means
    // audible crackle/distortion, distinct from an underrun -- this proves it directly instead
    // of guessing from theory.
    std::uint32_t clipCount() const
    {
        return _clipCount;
    }

private:
    static constexpr float sampleScale{32'768.0F};

    std::int16_t toInt16(float sample)
    {
        const float unclamped = sample * sampleScale;
        if (unclamped > 32'767.0F || unclamped < -32'768.0F)
        {
            ++_clipCount;
        }
        const auto scaled = std::clamp(unclamped, -32'768.0F, 32'767.0F);
        return static_cast<std::int16_t>(scaled);
    }

    ProcessorChain<Processors...>& _chain;
    std::array<float, MaxFramesPerHalf> _left{};
    std::array<float, MaxFramesPerHalf> _right{};
    std::uint32_t _clipCount{};
};

} // namespace galerna::core
