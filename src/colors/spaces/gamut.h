// SPDX-License-Identifier: GPL-2.0-or-later
//
// CSS Level 4 gamut mapping: https://www.w3.org/TR/css-color-4/#gamut-mapping
//
// Implementation adopted from:
// https://github.com/color-js/color.js/blob/main/src/toGamut.js

#ifndef SEEN_COLORS_SPACES_GAMUT_H
#define SEEN_COLORS_SPACES_GAMUT_H

#include <array>

#include "base.h"

namespace Inkscape::Colors {

// RGB gamuts a color can be tested against
enum class Gamut { sRGB, DisplayP3, Rec2020 };

/**
 * Given a color `origin`, returns a new color that is in gamut using
 * the CSS Gamut Mapping Algorithm. If `space` is specified, it will be in gamut
 * in `space`, and returned in `space`. Otherwise, it will be in gamut and
 * returned in the color space of `origin`.
 * @param {ColorTypes} origin
 * @param {{ space?: string | ColorSpace | undefined }} param1
 * @returns {PlainColorObject}
 */
Color to_gamut_css(const Color& origin, const std::shared_ptr<Space::AnySpace>& space);

// Check if color is outside of given color space gamut
bool out_of_gamut(const Color& color, const std::shared_ptr<Space::AnySpace>& space, double eps = 0.0001);

// Convert a linear sRGB color to the linear RGB of 'gamut' (D65)
std::array<double, 3> linear_srgb_to(const std::array<double, 3>& linear_srgb, Gamut gamut);

// Signed distance of a linear sRGB color from the edge of 'gamut' in that
// gamut's linear RGB: >= 0 inside, < 0 outside
double gamut_margin(const std::array<double, 3>& linear_srgb, Gamut gamut);

} // namespace Inkscape::Colors

#endif // SEEN_COLORS_SPACES_GAMUT_H
