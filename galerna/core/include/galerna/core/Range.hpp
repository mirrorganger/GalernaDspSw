#pragma once

#include <algorithm>
#include <cmath>

namespace galerna::core
{

// A closed [min, max] bound pair, plus the two normalized-0..1-to-range mapping shapes that were
// duplicated next to nearly every minXxx/maxXxx constexpr pair in this codebase (filter cutoff,
// decay time, evolve rate, reverb size, blink rate, ...). Callers whose normalized control runs
// the opposite way (higher normalized -> lower value, e.g. DriftVoice::setEvolveRate's "density"
// shortening an interval) pass 1.0F - normalized rather than needing a separate method: e.g.
// range.exponential(1.0F - normalized) is exactly max*(min/max)^normalized algebraically, just
// expressed against the same increasing-with-normalized map every other caller uses.
struct Range
{
    float min;
    float max;

    // normalized: 0..1 -> min..max, equal ratio per step rather than equal difference -- the
    // shape every filter-cutoff/time-constant control in this codebase uses (e.g. a timbre pot
    // shouldn't sound "halfway" between min and max the way a linear map would at 0.5).
    float exponential(float normalized) const
    {
        return min * std::pow(max / min, normalized);
    }

    // normalized: 0..1 -> min..max, equal step per step.
    float linear(float normalized) const
    {
        return min + normalized * (max - min);
    }

    // Clamps value into [min, max].
    float clamp(float value) const
    {
        return std::clamp(value, min, max);
    }
};

} // namespace galerna::core
