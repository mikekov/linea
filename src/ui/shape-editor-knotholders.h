// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SHAPE_EDITOR_KNOTHOLDERS_H
#define SEEN_SHAPE_EDITOR_KNOTHOLDERS_H

#include <memory>

class KnotHolder;
class SPItem;
class SPDesktop;

namespace Inkscape::UI {
std::unique_ptr<KnotHolder> create_knot_holder(SPItem *item, SPDesktop *desktop, double edit_rotation = 0.0,
                                               int edit_marker_mode = -1);

std::unique_ptr<KnotHolder> create_LPE_knot_holder(SPItem *item, SPDesktop *desktop);
} // namespace Inkscape::UI

#endif
