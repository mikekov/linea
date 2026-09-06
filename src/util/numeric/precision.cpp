// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Stringstream internal linking
 *
 * Copyright (C) 2026 AUTHORS
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "precision.h"
#include "preferences.h"

// WARNING: Do not include this file in Unit Testing! Use the mock file instead.

namespace Inkscape::Util {

int get_default_numeric_precision()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    return prefs->getInt("/options/svgoutput/numericprecision", 8);
}

int get_opacity_default_precision() {
    Inkscape::Preferences* prefs = Inkscape::Preferences::get();
    return prefs->getInt("/options/svgoutput/opacity-precision", 3);
}

} // namespace Inkscape::Util
