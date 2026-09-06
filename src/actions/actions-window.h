// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for window handling tied to the application and with GUI.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_WINDOW_H
#define INK_ACTIONS_WINDOW_H

class LineaApplication;
class InkscapeApplication;

void add_actions_window(InkscapeApplication* app);

void add_actions_window(LineaApplication* app);

#endif // INK_ACTIONS_WINDOW_H
