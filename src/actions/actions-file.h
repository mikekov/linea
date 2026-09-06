// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for file handling tied to the application and without GUI.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_FILE_H
#define INK_ACTIONS_FILE_H

class LineaApplication;
class LineaWindow;

void add_actions_file(LineaApplication* app);

void add_actions_file(LineaWindow* wnd);

#endif // INK_ACTIONS_FILE_H
