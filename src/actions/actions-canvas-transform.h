// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for transforming the canvas view. Tied to a particular LineaWindow.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_CANVAS_TRANSFORM_H
#define INK_ACTIONS_CANVAS_TRANSFORM_H

class LineaWindow;
class SPDesktop;

void add_actions_canvas_transform(LineaWindow* win);
void apply_preferences_canvas_transform(SPDesktop *desktop);

#endif // INK_ACTIONS_CANVAS_TRANSFORM_H
