// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Undo/Redo tied to document.
 *
 * Authors:
 *   Tavmjong Bah
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <array>
#include <giomm.h>

#include "actions-undo-document.h"
#include "actions-helper.h"
#include "action-registry.h"
#include "actions/action-meta.h"

#include "document.h"
#include "document-undo.h"
#include "i18n/action-strings.h"
#include "linea-application.h"
#include "linea-window.h"

namespace {

void undo(LineaWindow* win) {
    if (auto document = win->get_document()) {
        Inkscape::DocumentUndo::undo(document);
    }
}

void redo(LineaWindow* win) {
    if (auto document = win->get_document()) {
        Inkscape::DocumentUndo::redo(document);
    }
}

const Glib::ustring SECTION = NC_("Action Section", "Edit Document");

static auto undo_document_action_defs = std::to_array<WindowActionDef>({
    // clang-format off
    {"undo", N_("Undo"), SECTION, N_("Undo last action"), undo},
    {"redo", N_("Redo"), SECTION, N_("Do again the last undone action"), redo}
    // clang-format on
});

} // namespace

void enable_undo_actions(SPDocument* document, bool undo, bool redo) {
    auto& registry = ActionRegistry::get();
    if (registry.hasAction("undo") && registry.hasAction("redo")) {
        registry.action("undo")->setEnabled(undo);
        registry.action("redo")->setEnabled(redo);
    }
}

void add_actions_undo_document(LineaWindow* win) {
    auto& registry = ActionRegistry::get();

    for (auto& e : undo_document_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }
}
