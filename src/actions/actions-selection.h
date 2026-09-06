// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for selection tied to the application and without GUI.
 *
 * Copyright (C) 2018 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_SELECTION_H
#define INK_ACTIONS_SELECTION_H

class LineaApplication;

void add_actions_selection(LineaApplication* app);

#endif // INK_ACTIONS_SELECTION_H
