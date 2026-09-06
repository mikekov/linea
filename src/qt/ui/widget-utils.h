// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#ifndef LINEA_UI_WIDGET_UTILS_H
#define LINEA_UI_WIDGET_UTILS_H

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

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_UTILS_H
