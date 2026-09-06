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

#ifndef SEEN_MONITOR_H
#define SEEN_MONITOR_H

#include <QRect>

class QScreen;

namespace Linea::UI {

QRect get_monitor_geometry_primary();
QRect get_monitor_geometry_at_surface(QScreen *screen);
QRect get_monitor_geometry_at_point(int x, int y);

/**
 * Resolve the effective default import DPI from the
 * "/dialogs/import/defaultxdpi/value" preference.
 *
 * A value of 0 means "auto": the DPI is derived from the primary screen's
 * device pixel ratio (96 * dpr, e.g. 192 on a 2x display). Any other value is
 * returned as-is.
 */
double get_effective_default_dpi();

} // namespace Linea::UI

#endif // SEEN_MONITOR_H
