// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Entry points for event distribution
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_DESKTOP_EVENTS_H
#define INKSCAPE_DESKTOP_EVENTS_H

class SPDesktop;
class SPGuide;

namespace Inkscape {
class CanvasItemGuideLine;
struct CanvasEvent;
namespace UI::Widget {
class Canvas;
}
}

/* Item handlers */

bool sp_desktop_root_handler(Inkscape::CanvasEvent const &event, SPDesktop *desktop);

/* Guides */

bool sp_dt_guide_event(Inkscape::CanvasEvent const &event, Inkscape::CanvasItemGuideLine *guide_item, SPGuide *guide);

/// Begin dragging an existing guide from outside the normal press handler
/// (e.g. from a ruler drag). Looks up the guide's canvas item for the given
/// canvas, sets drag_type, and grabs it so that subsequent motion/release
/// events are routed to sp_dt_guide_event.
void sp_dt_guide_begin_drag(SPGuide* guide, Inkscape::UI::Widget::Canvas* canvas, int drag_type);

#endif // INKSCAPE_DESKTOP_EVENTS_H
