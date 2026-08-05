#pragma once

#include <cstddef>
#include <span>

namespace galerna::core
{

struct AudioBuffer
{
    std::span<float> left;
    std::span<float> right;

    [[nodiscard]] constexpr std::size_t size() const
    {
        return left.size() < right.size() ? left.size() : right.size();
    }
};

} // namespace galerna::core
