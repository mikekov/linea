// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for selection tied to the application and without GUI.
 *
 * Copyright (C) 2023 Martin Owens
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_HELPER_GUI_H
#define INK_ACTIONS_HELPER_GUI_H

#include "actions-helper.h"

class LineaWindow;

void activate_any_actions(action_vector_t const &actions, Glib::RefPtr<Gio::Application> app, LineaWindow *win, SPDocument *doc);

#endif // INK_ACTIONS_HELPER_GUI_H
