// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for dialogs.
 *
 * Copyright (C) 2021 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_DIALOGS_H
#define INK_ACTIONS_DIALOGS_H

class InkscapeApplication;
class LineaWindow;

// Standard function to add actions.
void add_actions_dialogs(InkscapeApplication *app);
void add_actions_dialogs(LineaWindow *win);

#endif // INK_ACTIONS_DIALOGS_H
