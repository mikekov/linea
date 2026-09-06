// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions related to hide and lock
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *   Tavmjong Bah
 *
 * Copyright (C) 2021-2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-hide-lock.h"
#include "actions/action-meta.h"
#include "actions-helper.h"

#include <array>
#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include "i18n/action-strings.h"

#include "action-registry.h"
#include "document.h"
#include "document-undo.h"
#include "linea-application.h"
#include "selection.h"

#include "object/sp-root.h"

namespace ActionsHideLock {

// Helper to unlock/unhide everything. (Could also be used to lock/hide everything but that isn't very useful.)
static bool
hide_lock_recurse(bool (*f)(SPItem*, bool), SPItem *item, bool hide_or_lock)
{
    bool changed = false;

    if (f(item, hide_or_lock)) {
        changed = true;
    }

    for (auto& child : item->children) {
        auto item = cast<SPItem>(&child);
        if (item && hide_lock_recurse(f, item, hide_or_lock)) {
            changed = true;
        }
    }

    return changed;
}

// Helper to hide/unhide one item.
bool
hide_lock_hide(SPItem* item, bool hide)
{
    bool changed = false;
    if (item->isHidden() != hide) {
        item->setHidden(hide);
        changed = true;
    }
    return changed;
}

// Helper to lock/unlock one item.
bool
hide_lock_lock(SPItem* item, bool lock)
{
    bool changed = false;
    if (item->isLocked() != lock) {
        item->setLocked(lock);
        changed = true;
    }
    return changed;
}

// Unhide all
void
hide_lock_unhide_all(LineaApplication* app)
{
    auto document = app->get_active_document();
    if (!document) {
        return;
    }
    auto root = document->getRoot();

    bool changed = hide_lock_recurse(&hide_lock_hide, root, false); // Unhide

    if (changed) {
        Inkscape::DocumentUndo::done(document, RC_("Undo", "Unhid all objects in the current layer"), "");
    }
}

// Unlock all
void
hide_lock_unlock_all(LineaApplication* app)
{
    auto document = app->get_active_document();
    if (!document) {
        return;
    }
    auto root = document->getRoot();

    bool changed = hide_lock_recurse(&hide_lock_lock, root, false); // Unlock

    if (changed) {
        Inkscape::DocumentUndo::done(document, RC_("Undo", "Unlocked all objects in the current layer"), "");
    }
}

// Unhide selected items and their descendents.
void
hide_lock_unhide_below(LineaApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    bool changed = false;
    for (auto item : selection->items()) {
        if (hide_lock_recurse(&hide_lock_hide, item, false)) {
            changed = true;
        }
    }

    if (changed) {
        Inkscape::DocumentUndo::done(document, RC_("Undo", "Unhid selected items and their descendents."), "");
    }
}

// Unlock selected items and their descendents.
void
hide_lock_unlock_below(LineaApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    bool changed = false;
    for (auto item : selection->items()) {
        if (hide_lock_recurse(&hide_lock_lock, item, false)) {
            changed = true;
        }
    }

    if (changed) {
        Inkscape::DocumentUndo::done(document, RC_("Undo", "Unlocked selected items and their descendents."), "");
    }
}

// Hide/unhide selected items.
void
hide_lock_hide_selected(LineaApplication* app, bool hide)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        show_output("hide_lock_hide_selected: no selection!");
        return;
    }

    bool changed = false;
    for (auto item : selection->items()) {
        if (hide_lock_hide(item, hide)) {
            changed = true;
        }
    }

    if (changed) {
        auto document = app->get_active_document();
        if (!document) {
            return;
        }
        Inkscape::DocumentUndo::done(document, (hide ? RC_("Undo", "Hid selected items.") : RC_("Undo", "Unhid selected items.")), "");
        selection->clear();
    }
}

// Lock/Unlock selected items.
void
hide_lock_lock_selected(LineaApplication* app, bool lock)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        show_output("hide_lock_lock_selected: no selection!");
        return;
    }

    bool changed = false;
    for (auto item : selection->items()) {
        if (hide_lock_lock(item, lock)) {
            changed = true;
        }
    }

    if (changed) {
        auto document = app->get_active_document();
        if (!document) {
            return;
        }
        Inkscape::DocumentUndo::done(document, (lock ? RC_("Undo", "Locked selected items.") : RC_("Undo", "Unlocked selected items.")), "");
        selection->clear();
    }
}

void selection_hide(LineaApplication* app)      { hide_lock_hide_selected(app, true);  }
void selection_unhide(LineaApplication* app)    { hide_lock_hide_selected(app, false); }
void selection_lock(LineaApplication* app)      { hide_lock_lock_selected(app, true);  }
void selection_unlock(LineaApplication* app)    { hide_lock_lock_selected(app, false); }

const Glib::ustring SECTION = NC_("Action Section", "Hide and Lock");

static auto hide_lock_action_defs = std::to_array<ApplicationActionDef>({
    // clang-format off
    {"unhide-all",              N_("Unhide All"),         SECTION, N_("Unhide all objects"),                           hide_lock_unhide_all},
    {"unlock-all",              N_("Unlock All"),         SECTION, N_("Unlock all objects"),                           hide_lock_unlock_all},
    {"selection-hide",          N_("Hide selection"),     SECTION, N_("Hide all selected objects"),                    selection_hide},
    {"selection-unhide",        N_("Unhide selection"),   SECTION, N_("Unhide all selected objects"),                  selection_unhide},
    {"selection-unhide-below",  N_("Unhide descendents"), SECTION, N_("Unhide all items inside selected objects"),     hide_lock_unhide_below},
    {"selection-lock",          N_("Lock selection"),     SECTION, N_("Lock all selected objects"),                    selection_lock},
    {"selection-unlock",        N_("Unlock selection"),   SECTION, N_("Unlock all selected objects"),                  selection_unlock},
    {"selection-unlock-below",  N_("Unlock descendents"), SECTION, N_("Unlock all items inside selected objects"),     hide_lock_unlock_below},
    // clang-format on
});

} // namespace ActionsHideLock

using namespace ActionsHideLock;

void
add_actions_hide_lock(LineaApplication* app)
{
    auto& registry = ActionRegistry::get();

    for (auto& e : hide_lock_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, app]() { fn(app); });
        app->get_active_window()->addAction(a);
    }
}
