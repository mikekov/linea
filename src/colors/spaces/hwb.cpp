// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "hwb.h"

#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/**
 * Print the HWB color to a CSS hwb() string.
 *
 * @arg values - A vector of doubles for each channel in the HWB space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string HWB::toString(const std::vector<double>& values, bool opacity) const {
    static constexpr double CSS_WB_SCALE = 100.0;
    auto oo = CssFuncPrinter(3, "hwb");

    oo << (int)(values[0] * 360)    // Hue,      degrees, 0..360
       << values[1] * CSS_WB_SCALE  // Whiteness,        0..100
       << values[2] * CSS_WB_SCALE; // Blackness,       0..100

    if (opacity && values.size() == 4) oo << values[3]; // Optional opacity

    return oo;
}

}; // namespace Inkscape::Colors::Space
