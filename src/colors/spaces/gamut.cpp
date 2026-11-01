// SPDX-License-Identifier: GPL-2.0-or-later

#include "gamut.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"
#include "colors/spaces/enum.h"

namespace Inkscape::Colors {

const auto oklab_space = Manager::get().find(Space::Type::OKLAB);
const auto oklch_space = Manager::get().find(Space::Type::OKLCH);
const Color COLORS_WHITE = Color::rgb(0xffffff);
const Color COLORS_BLACK = Color::rgb(0x000000);

namespace {

bool _out_of_gamut(const std::vector<double>& input, const Space::AnySpace& space, double eps) {
    if (input.empty()) return false;

    const auto N = space.getComponentCount();
    if (input.size() < N) {
        throw ColorError("_out_of_gamut: color values count doesn't match number of components");
    }

    // simple check for channels outside of 0..1 range, since most channels use normalized ranges;
    // using epsilon value to ignore conversion rounding errors
    for (int i = 0; i < N; ++i) {
        if (input[i] < 0 - eps || input[i] > 1 + eps) {
            return true; // out of gamut
        }
    }
    return false; // in gamut
}

/**
 * More accurate color-difference formulae
 * than the simple 1976 Euclidean distance in CIE Lab
 * @param {import("../types.js").ColorTypes} color
 * @param {import("../types.js").ColorTypes} sample
 * @returns {number}
 */
double deltaEOK(const Color& color, const Color& sample) {
	// Given this color as the reference and a sample,
	// calculate deltaEOK, term by term as root sum of squares
    auto c = *color.converted(oklab_space);
    auto s = *sample.converted(oklab_space);

    auto L1 = c[0];
    auto a1 = c[1];
    auto b1 = c[2];
    auto L2 = s[0];
    auto a2 = s[1];
    auto b2 = s[2];

	auto ΔL = L1 - L2;
	auto Δa = a1 - a2;
	auto Δb = b1 - b2;
	return std::sqrt(ΔL * ΔL + Δa * Δa + Δb * Δb);
}

// Linear sRGB -> linear gamut RGB matrices (D65 white point)
constexpr std::array<std::array<double, 3>, 3> LINEAR_SRGB_TO_P3 = {{
    {0.8224619687, 0.1775380313, 0},
    {0.0331941989, 0.9668058011, 0},
    {0.0170826307, 0.0723974407, 0.9105199286}
}};
constexpr std::array<std::array<double, 3>, 3> LINEAR_SRGB_TO_REC2020 = {{
    {0.6274038959, 0.3292830384, 0.0433130657},
    {0.0690972894, 0.9195403951, 0.0113623156},
    {0.0163914389, 0.0880133079, 0.8955952532}
}};

double min_channel_margin(const std::array<double, 3>& rgb) {
    double margin = std::numeric_limits<double>::max();
    for (double c : rgb) {
        margin = std::min(margin, std::min(c, 1.0 - c));
    }
    return margin;
}

} // namespace

std::array<double, 3> linear_srgb_to(const std::array<double, 3>& linear_srgb, Gamut gamut) {
    if (gamut == Gamut::sRGB) {
        return linear_srgb;
    }
    const auto& m = gamut == Gamut::DisplayP3 ? LINEAR_SRGB_TO_P3 : LINEAR_SRGB_TO_REC2020;
    std::array<double, 3> rgb;
    for (int r = 0; r < 3; ++r) {
        rgb[r] = m[r][0] * linear_srgb[0] + m[r][1] * linear_srgb[1] + m[r][2] * linear_srgb[2];
    }
    return rgb;
}

double gamut_margin(const std::array<double, 3>& linear_srgb, Gamut gamut) {
    return min_channel_margin(linear_srgb_to(linear_srgb, gamut));
}

Color to_gamut_css(const Color& origin, const std::shared_ptr<Space::AnySpace>& space) {
    if (!space) throw std::runtime_error("to_gamut_css: missing color space");

    constexpr double JND = 0.02;
    constexpr double ε = 0.0001;

    if (space->isUnbounded()) {
        return *origin.converted(space);
    }

    const auto origin_OKLCH = *origin.converted(oklch_space);
    const double L = origin_OKLCH[0];

    // return media white or black, if lightness is out of range
    if (L >= 1) {
        auto white = *COLORS_WHITE.converted(space);
        if (origin.hasOpacity()) {
            white.setOpacity(origin.getOpacity());
        }
        return white;
    }
    if (L <= 0) {
        auto black = *COLORS_BLACK.converted(space);
        if (origin.hasOpacity()) {
            black.setOpacity(origin.getOpacity());
        }
        return black;
    }

    if (!out_of_gamut(origin_OKLCH, space, 0/*{epsilon: 0}*/)) {
        return *origin_OKLCH.converted(space);
    }

    auto clip = [&space](auto& _color) {
        auto destColor = *_color.converted(space);
        destColor.normalize();
        return destColor;
    };

    double min = 0;
    double max = origin_OKLCH[1];
    bool min_inGamut = true;
    auto current = origin_OKLCH;
    auto clipped = clip(current);

    auto E = deltaEOK(clipped, current);
    if (E < JND) {
        return clipped;
    }

    while ((max - min) > ε) {
        const auto chroma = (min + max) / 2;
        current.set(1, chroma);
        if (min_inGamut && !out_of_gamut(current, space, 0/*{epsilon: 0}*/)) {
            min = chroma;
        }
        else {
            clipped = clip(current);
            E = deltaEOK(clipped, current);
            if (E < JND) {
                if ((JND - E < ε)) {
                    break;
                }
                else {
                    min_inGamut = false;
                    min = chroma;
                }
            }
            else {
                max = chroma;
            }
        }
    }
    return clipped;
}

bool out_of_gamut(const Color& color, const std::shared_ptr<Space::AnySpace>& space, double eps) {
    // if space is unbounded, all values are considered in-gamut
    if (space->isUnbounded()) return false;

    if (color.getSpace() == space) {
        return _out_of_gamut(color.getValues(), *space, eps);
    }
    else {
        return _out_of_gamut(color.converted(space)->getValues(), *space, eps);
    }
}

} // namespace
