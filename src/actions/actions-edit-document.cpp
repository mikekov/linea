// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions Related to Editing which require document
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-edit-document.h"

#include <glibmm/i18n.h>
#include <glibmm/ustring.h>

#include "action-meta.h"
#include "action-registry.h"
#include "document-undo.h"
#include "document.h"
#include "linea-window.h"
#include "object/sp-guide.h"
#include "object/sp-namedview.h"
#include "selection-chemistry.h"

// TODO: Add to action table with UI metadata and state query when porting display unit actions
void set_display_unit(Glib::ustring abbr, SPDocument* document) {
    // This does not modify the scale of the document, just the units
    document->getNamedView()->setDisplayUnit(abbr);
    // Inkscape::XML::Node* repr = document->getNamedView()->getRepr();
    // repr->setAttribute("inkscape:document-units", abbr);
    document->setModifiedSinceSave();
    Inkscape::DocumentUndo::done(document, RC_("Undo", "Changed default display unit"), "");
}

namespace {

void create_guides_around_page(SPDocument* document) {
    // Create Guides Around the Page
    sp_guide_create_guides_around_page(document);
}

void lock_all_guides(SPDocument* document) {
    document->getNamedView()->toggleLockGuides();
}

bool are_guides_visible(SPDocument* document) {
    return document->getNamedView()->getShowGuides();
}

void show_all_guides(SPDocument* document) {
    document->getNamedView()->toggleShowGuides();
}

void delete_all_guides(SPDocument* document) {
    // Delete All Guides
    sp_guide_delete_all_guides(document);
}

void fit_canvas_drawing(SPDocument* document) {
    // Fit Page to Drawing
    if (fit_canvas_to_drawing(document)) {
        Inkscape::DocumentUndo::done(document, RC_("Undo", "Fit Page to Drawing"), "");
    }
}

void toggle_clip_to_page(SPDocument* document) {
    if (!document->getNamedView()) return;
    auto clip = !document->getNamedView()->clip_to_page;
    document->getNamedView()->change_bool_setting(SPAttr::INKSCAPE_CLIP_TO_PAGE_RENDERING, clip);
    document->setModifiedSinceSave();
    Inkscape::DocumentUndo::done(document, RC_("Undo", "Clip to page"), "");
}

bool is_clip_to_page_active(SPDocument* document) {
    return document->getNamedView()->clip_to_page;
}

void show_grids(SPDocument* document) {
    document->getNamedView()->toggleShowGrids();
}

bool are_grids_visible(SPDocument* document) {
    return document->getNamedView()->getShowGrids();
}

// Wraps a DocCallback: fetches the active document from win at trigger time.
// If there is no document the action is a no-op.
template <typename Fn>
auto doc_action(LineaWindow* win, Fn fn) {
    return [win, fn]() {
        if (auto* doc = win->get_document()) {
            fn(doc);
        }
    };
}

// Wraps a state query function: fetches the active document from win at query time.
// Returns false if there is no document.
template <typename Fn>
auto doc_state_query(LineaWindow* win, Fn fn) {
    return [win, fn]() -> bool {
        if (auto* doc = win->get_document()) {
            return fn(doc);
        }
        return false;
    };
}

struct EditDocEntry {
    BoolActionMeta meta;
    DocCallback fn;
    bool (*state_fn)(SPDocument*);
};

const ActionGroup editDocActionGroup = {"edit-document", N_("Edit Document"), ActionScope::Document, {}};

const EditDocEntry editDocTable[] = {
    // clang-format off
    {{"create-guides-around-page", N_("Create Guides Around the Current Page"), N_("Create four guides aligned with the page borders of the current page"), "guide",       nullptr}, create_guides_around_page, nullptr},
    {{"lock-all-guides",           N_("Lock All Guides"),                       N_("Toggle lock of all guides in the document"),                            "guide",       N_("Unlock All Guides")}, lock_all_guides,           nullptr},
    {{"show-all-guides",           N_("Show All Guides"),                       N_("Toggle visibility of all guides in the document"),                       "show-guides", N_("Hide All Guides")}, show_all_guides,           are_guides_visible },
    {{"delete-all-guides",         N_("Delete All Guides"),                     N_("Delete all the guides in the document"),                                "guide",       nullptr}, delete_all_guides,         nullptr},
    {{"fit-canvas-to-drawing",     N_("Fit Page to Drawing"),                   N_("Fit the page to the drawing"),                                          "pages-resize",nullptr}, fit_canvas_drawing,        nullptr},
    {{"clip-to-page",              N_("Clip to Page"),                          N_("Toggle between clipped to page and complete rendering"),                 "page",        N_("Unclip to Page")}, toggle_clip_to_page,       is_clip_to_page_active },
    {{"show-grids",                N_("Show Grids"),                            N_("Toggle the visibility of grids"),                                       "show-grid",   N_("Hide Grids")}, show_grids,                are_grids_visible },
    // clang-format on
};

} // namespace

void add_actions_edit_document(LineaWindow* win) {
    auto& registry = ActionRegistry::get();
    registry.registerGroup(editDocActionGroup);

    for (auto& e : editDocTable) {
        QAction* a;
        if (e.state_fn) {
            a = registry.createBoolAction(e.meta,
                [fn = e.fn, win](bool) { doc_action(win, fn)(); },
                [state_fn = e.state_fn, win]() { return doc_state_query(win, state_fn)(); });
        } else {
            a = registry.createAction(e.meta, [fn = e.fn, win]() { doc_action(win, fn)(); });
        }
        win->addAction(a);
    }
}
