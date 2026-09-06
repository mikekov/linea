// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_EVENTS_SCROLL_UNIT_H
#define INKSCAPE_UI_WIDGET_EVENTS_SCROLL_UNIT_H

namespace Inkscape {

/**
 * The units of a scroll delta, mirroring Gdk::ScrollUnit.
 */
enum class ScrollUnit {
    WHEEL,
    SURFACE
};

} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_EVENTS_SCROLL_UNIT_H
