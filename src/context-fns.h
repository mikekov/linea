// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CONTEXT_FNS_H
#define SEEN_CONTEXT_FNS_H

/*
 *
 * Authors:
 *
 * Copyright (C)
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/forward.h>

class SPDesktop;
class SPItem;

inline constexpr double goldenratio = 1.61803398874989484820;

namespace Inkscape {

class MessageContext;
class MessageStack;

bool have_viable_layer(SPDesktop *desktop, MessageContext *message);
bool have_viable_layer(SPDesktop *desktop, MessageStack *message);
Geom::Rect snap_rectangular_box(SPDesktop const *desktop, SPItem *item,
                                Geom::Point const &pt, Geom::Point const &center, int state);

} // namespace Inkscape

#endif // !SEEN_CONTEXT_FNS_H
