// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#ifndef LINEA_UI_WIDGET_UTILS_H
#define LINEA_UI_WIDGET_UTILS_H

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

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_UTILS_H
