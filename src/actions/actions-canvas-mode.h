// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for changing the canvas display mode. Tied to a particular LineaWindow.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_CANVAS_MODE_H
#define INK_ACTIONS_CANVAS_MODE_H

class LineaApplication;
class SPDesktop;

void add_actions_canvas_mode(LineaApplication* app);

#endif // INK_ACTIONS_CANVAS_MODE_H
