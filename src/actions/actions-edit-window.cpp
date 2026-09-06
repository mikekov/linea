// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Editing an object which require desktop
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-edit-window.h"

#include <array>
#include <giomm.h>

#include "action-registry.h"
#include "actions-helper.h"
#include "actions/action-meta.h"
#include "desktop.h"
#include "i18n/action-strings.h"
#include "linea-window.h"
#include "selection-chemistry.h"

namespace {

void paste(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }
    sp_selection_paste(dt, false);
}

void paste_in_place(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }
    sp_selection_paste(dt, true);
}

void paste_on_page(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }
    sp_selection_paste(dt, true, true);
}

void path_effect_parameter_next(LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }
    sp_selection_next_patheffect_param(dt);
}

const Glib::ustring SECTION = NC_("Action Section", "Edit");

static auto edit_window_action_defs = std::to_array<WindowActionDef>({
    // clang-format off
    {"paste",                       N_("Paste"),                     SECTION, N_("Paste objects from clipboard to mouse point, or paste text"), paste},
    {"paste-in-place",              N_("Paste in Place"),            SECTION, N_("Paste objects from clipboard to the original position of the copied objects"), paste_in_place},
    {"paste-on-page",               N_("Paste on Page"),             SECTION, N_("Paste objects from clipboard into the same place on the selected page."), paste_on_page},
    {"path-effect-parameter-next",  N_("Next Path Effect Parameter"), SECTION, N_("Show next editable path effect parameter"), path_effect_parameter_next}
    // clang-format on
});

} // namespace

void add_actions_edit_window(LineaWindow* win) {
    auto& registry = ActionRegistry::get();

    for (auto& e : edit_window_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }
}
