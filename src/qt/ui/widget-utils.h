// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#ifndef LINEA_UI_WIDGET_UTILS_H
#define LINEA_UI_WIDGET_UTILS_H

#include <QPoint>
#include <QSize>

class QWidget;
class QLayout;

namespace Linea::UI {

/**
 * Apply left/right padding as content margins to a widget's layout, leaving
 * the top/bottom margins untouched. Used to inset child panels of properties
 * panels. Does nothing if the widget has no layout.
 */
void applyHorizontalPadding(QWidget* widget, int left, int right);

/**
 * Keep `follower`'s visibility in sync with `leader`'s. An event filter is
 * installed on `leader` so that any subsequent setVisible() call on it (e.g.
 * by the Binder's visibility rules) is mirrored onto `follower`. The follower
 * is also synced immediately to the leader's current state.
 *
 * The filter object is parented to `follower`, so it is destroyed with it.
 */
void syncVisibility(QWidget* follower, QWidget* leader);

/**
 * Restore `window`'s position and size from preferences under `prefsPath`
 * (keys x, y, width, height) and persist them on subsequent move, resize,
 * and close. If the saved position lands on a display that is not
 * connected — or leaves the title bar off-screen — the window is moved
 * back onto a visible screen (largest overlap, else the parent's screen).
 *
 * `defaultSize` is used when no usable size was stored yet; pass an
 * invalid QSize to keep the widget's own default.
 *
 * An event filter is installed on `window`, parented to it, so the
 * tracking dies with the window.
 */
void persistGeometry(QWidget* window, const char* prefsPath, QSize defaultSize = {});

/**
 * Clamp `pos` so a popup of `size` placed there stays inside the available
 * geometry of the screen containing that point (primary screen if none),
 * keeping a small margin from the screen edges.
 */
void ensurePopupOnScreen(QPoint& pos, QSize size);

/**
 * Settle `widget`'s layout geometry synchronously after visibility changes.
 *
 * setVisible() takes effect at the next paint, but the geometry it implies
 * only settles when queued LayoutRequests are delivered on a later
 * event-loop pass — a repaint landing in between shows widgets at their old
 * positions with gaps where hidden rows were.
 *
 * Pending Polish/FontChange/StyleChange events are delivered to the parent
 * subtree first — QLabel caches its sizeHint until FontChange, so a layout
 * run before delivery computes stale geometry and re-runs (visibly) when
 * the events finally arrive. Then LayoutRequests are delivered to the
 * widget and each ancestor, innermost first — the same work the event loop
 * would do a frame later. (layout->activate() alone is NOT equivalent: it
 * does not re-assign widget sizes from updated sizeHints — the
 * LayoutRequest path does; and a global sendPostedEvents(nullptr, ...)
 * drains only the queue as-posted, while resizes during the flush post new
 * requests.)
 */
void settleLayout(QWidget* widget);

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_UTILS_H
