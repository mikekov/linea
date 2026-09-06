// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for selection tied to the application and without GUI.
 *
 * Copyright (C) 2023 Martin owens
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-helper-gui.h"

#include "document.h"
#include "linea-window.h"
#include "inkscape-application.h"

/**
 * This activates all actions, which are called even if you don't know or care about which action group it's in.
 */
void activate_any_actions(action_vector_t const &actions, Glib::RefPtr<Gio::Application> app, LineaWindow *win, SPDocument *doc)
{
    for (auto const &[name, param] : actions) {
        if (app->has_action(name)) {
            app->activate_action(name, param);
        } else if (win && win->has_action(name)) {
            win->activate_action(name, param);
        } else if (doc && doc->getActionGroup()->has_action(name)) {
            doc->getActionGroup()->activate_action(name, param);
        } else {
            std::cerr << "ActionsHelper::activate_actions: Unknown action name: " << name << std::endl;
        }
    }
}
