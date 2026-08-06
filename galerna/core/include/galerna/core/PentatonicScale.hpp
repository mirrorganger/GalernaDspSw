#pragma once

#include <array>
#include <cstddef>

namespace galerna::core
{

// Ratios of a pentatonic scale (root, major second, major third, perfect fifth, major sixth)
// relative to a root/lowest frequency -- shared by every voice class that quantizes a continuous
// 0..1 control (a pot reading, or a random pick) onto a musically consonant note. Multiple
// simultaneously-sounding voices built on this scale stay consonant regardless of which notes
// land together, since none of the ratios clash by a semitone (see galerna::effects::WindChimeVoice
// and galerna::effects::PluckVoice, both of which rely on that property when summing/gating
// several voices at once).
struct PentatonicScale
{
    static constexpr std::size_t degreeCount{5U};
    static constexpr std::array<float, degreeCount> ratios{
        1.0F, 9.0F / 8.0F, 5.0F / 4.0F, 3.0F / 2.0F, 5.0F / 3.0F};
};

} // namespace galerna::core
