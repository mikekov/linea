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

#include <array>
#include <glibmm/i18n.h>
#include <glibmm/ustring.h>

#include "action-meta.h"
#include "action-registry.h"
#include "document-undo.h"
#include "document.h"
#include "linea-application.h"
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

const ActionGroup editDocActionGroup = {"edit-document", N_("Edit Document"), ActionScope::Document};

const Glib::ustring SECTION = NC_("Action Section", "Edit Document");

static auto editDocTable = std::to_array<ActionSpec<SPDocument>>({
    // clang-format off
    {"create-guides-around-page", N_("Create Guides Around the Current Page"), SECTION, N_("Create four guides aligned with the page borders of the current page"), "guide",        create_guides_around_page},
    {"lock-all-guides",           N_("Lock All Guides"),                       SECTION, N_("Toggle lock of all guides in the document"),                            "guide",        lock_all_guides},
    {"show-all-guides",           N_("Show All Guides"),                       SECTION, N_("Toggle visibility of all guides in the document"),                      "show-guides",  show_all_guides,      are_guides_visible,      N_("Hide All Guides")},
    {"delete-all-guides",         N_("Delete All Guides"),                     SECTION, N_("Delete all the guides in the document"),                                "guide",        delete_all_guides},
    {"fit-canvas-to-drawing",     N_("Fit Page to Drawing"),                   SECTION, N_("Fit the page to the drawing"),                                          "pages-resize", fit_canvas_drawing},
    {"clip-to-page",              N_("Clip to Page"),                          SECTION, N_("Toggle between clipped to page and complete rendering"),                "page",         toggle_clip_to_page,  is_clip_to_page_active,  N_("Unclip to Page")},
    {"show-grids",                N_("Show Grids"),                            SECTION, N_("Toggle the visibility of grids"),                                       "show-grid",    show_grids,           are_grids_visible,       N_("Hide Grids")},
    // clang-format on
});

} // namespace

void add_actions_edit_document(LineaApplication* app) {
    auto& registry = ActionRegistry::get();
    registry.registerGroup(editDocActionGroup);
    registry.registerActions(app, editDocTable);
}
