// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Filters and Extension menu items
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-effect.h"

#include <giomm.h>
#include <glibmm/i18n.h>

#include "actions-helper.h"
#include "actions/action-registry.h"
#include "desktop.h"
#include "document.h"
#include "extension/db.h"
#include "extension/effect.h"
#include "linea-application.h"
#include "selection.h"

void edit_remove_filter(SPDesktop* desktop) {
    // Remove Filter
    desktop->getSelection()->removeFilter();
}

void last_effect(SPDesktop* desktop) {
    Inkscape::Extension::Effect* effect = Inkscape::Extension::Effect::get_last_effect();

    if (effect == nullptr) {
        return;
    }

    // Last Effect
    effect->effect(desktop);
}

void last_effect_pref(SPDesktop* desktop) {
    Inkscape::Extension::Effect* effect = Inkscape::Extension::Effect::get_last_effect();

    if (effect == nullptr) {
        return;
    }

    // Last Effect Pref
    effect->prefs(desktop);
}

void enable_effect_actions(LineaApplication* app, bool enabled) {
    /* Qt TODO
    auto gapp = app->gio_app();
    auto le_action = gapp->lookup_action("last-effect");
    auto lep_action = gapp->lookup_action("last-effect-pref");
    auto le_saction = std::dynamic_pointer_cast<Gio::SimpleAction>(le_action);
    auto lep_saction = std::dynamic_pointer_cast<Gio::SimpleAction>(lep_action);
    // GTK4
    // auto le_saction = dynamic_cast<Gio::SimpleAction*>(le_action);
    // auto lep_saction = dynamic_cast<Gio::SimpleAction*>(lep_action);
    if (!le_saction || !lep_saction) {
        g_warning("Unable to find Extension actions.");
        return;
    }
    // Enable/disable menu items.
    le_saction->set_enabled(enabled);
    lep_saction->set_enabled(enabled);
*/
}

const Glib::ustring SECTION_FILTERS = NC_("Action Section", "Filters");
const Glib::ustring SECTION_EXT = NC_("Action Section", "Extensions");

static auto effect_action_defs = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"edit-remove-filter",      N_("Remove Filters"),              SECTION_FILTERS, N_("Remove any filters from selected objects"),          nullptr, edit_remove_filter},
    {"last-effect",             N_("Previous Extension"),          SECTION_EXT,     N_("Repeat the last extension with the same settings"),  nullptr, last_effect},
    {"last-effect-pref",        N_("Previous Extension Settings"), SECTION_EXT,     N_("Repeat the last extension with new settings"),       nullptr, last_effect_pref},
    // clang-format on
});

void add_actions_effect(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, effect_action_defs);
}
