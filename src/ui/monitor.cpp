// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * \brief helper functions for retrieving monitor geometry, etc.
 *//*
 * Authors:
 * see git history
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "monitor.h"

#include "preferences.h"
#include "util/units.h"

#include <QGuiApplication>
#include <QScreen>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Linea::UI {

/** get monitor geometry of primary monitor */
QRect get_monitor_geometry_primary() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return {};
    }
    return screen->geometry();
}

/** get monitor geometry of monitor containing Qt screen */
QRect get_monitor_geometry_at_surface(QScreen* screen) {
    if (!screen) {
        return {};
    }
    return screen->geometry();
}

/** get monitor geometry of monitor at (or closest to) point on combined screen area */
QRect get_monitor_geometry_at_point(int x, int y) {
    QRect best;
    double best_dist = std::numeric_limits<double>::max();
    for (QScreen* screen : QGuiApplication::screens()) {
        QRect g = screen->geometry();
        double cdist =
            std::hypot(std::max({g.left() - x, 0, x - g.right()}), std::max({g.top() - y, 0, y - g.bottom()}));
        if (cdist < best_dist) {
            best = g;
            best_dist = cdist;
        }
    }
    return best;
}

double get_effective_default_dpi() {
    auto prefs = Inkscape::Preferences::get();
    double dpi = prefs->getDouble("/dialogs/import/defaultxdpi/value", 0.0);
    if (dpi <= 0) {
        // "auto": derive from the primary screen's device pixel ratio
        double dpr = 1.0;
        if (auto screen = QGuiApplication::primaryScreen()) {
            dpr = screen->devicePixelRatio();
        }
        dpi = Inkscape::Util::Quantity::convert(1, "in", "px") * dpr;
    }
    return dpi;
}

} // namespace Linea::UI
