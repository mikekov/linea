// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors: see git history
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_HWB_H
#define SEEN_COLORS_SPACES_HWB_H

#include <cmath>

#include "rgb.h"

namespace Inkscape::Colors::Space {

class HWB : public RGBBase<HWB> {
public:
    constexpr static int ProfileChannels = 3;
    constexpr static int OutputChannels = 3;

    HWB()
        : RGBBase(Type::HWB, "HWB", "HWB", "color-selector-hsx") {}
    ~HWB() override = default;

    /**
     * Convert the HWB color into sRGB components used in the sRGB icc profile.
     */
    template <typename T, bool PremultipliedAlpha = false>
    inline static void spaceToProfile(const T* i, T* o) {
        double h;
        double w;
        double b;

        if constexpr (PremultipliedAlpha) {
            h = i[0] / i[3];
            w = i[1] / i[3];
            b = i[2] / i[3];
            o[3] = i[3];
        } else {
            h = i[0];
            w = i[1];
            b = i[2];
        }

        // Out of gamut values are a shade of grey, per the hwb definition
        if (double sum = w + b; sum >= 1.0) {
            o[0] = o[1] = o[2] = w / sum;
            return;
        }

        // The fully saturated rgb color at this hue, mixed to taste
        double d = h * 5.99999999;
        double f = d - std::floor(d);
        double c[3];

        if (d < 1.0) {
            c[0] = 1.0;
            c[1] = f;
            c[2] = 0.0;
        } else if (d < 2.0) {
            c[0] = 1.0 - f;
            c[1] = 1.0;
            c[2] = 0.0;
        } else if (d < 3.0) {
            c[0] = 0.0;
            c[1] = 1.0;
            c[2] = f;
        } else if (d < 4.0) {
            c[0] = 0.0;
            c[1] = 1.0 - f;
            c[2] = 1.0;
        } else if (d < 5.0) {
            c[0] = f;
            c[1] = 0.0;
            c[2] = 1.0;
        } else {
            c[0] = 1.0;
            c[1] = 0.0;
            c[2] = 1.0 - f;
        }

        double chroma = 1.0 - w - b;
        o[0] = c[0] * chroma + w;
        o[1] = c[1] * chroma + w;
        o[2] = c[2] * chroma + w;
    }

    /**
     * Convert from sRGB icc values to HWB values
     */
    template <typename T, bool PremultiplyAlpha = false>
    inline static void profileToSpace(const T* i, T* o) {
        double r = i[0];
        double g = i[1];
        double b = i[2];

        double max = std::max(std::max(r, g), b);
        double min = std::min(std::min(r, g), b);
        double delta = max - min;

        o[1] = min;       // whiteness
        o[2] = 1.0 - max; // blackness

        if (delta > ACHROMATIC_RGB_DELTA) {
            if (r == max) {
                o[0] = (g - b) / delta;
            } else if (g == max) {
                o[0] = 2.0 + (b - r) / delta;
            } else {
                o[0] = 4.0 + (r - g) / delta;
            }
            o[0] = o[0] / 6.0;
            if (o[0] < 0) o[0] += 1.0;
        } else
            o[0] = 0.0;

        if constexpr (PremultiplyAlpha) {
            o[0] *= i[3];
            o[1] *= i[3];
            o[2] *= i[3];
            o[3] = i[3];
        }
    }

    std::string toString(const std::vector<double>& values, bool opacity) const override;

    class Parser : public HueParser {
    public:
        Parser(bool alpha)
            : HueParser("hwb", Space::Type::HWB, alpha, 100.0) {}
    };
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_HWB_H
