// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt Actions for pages.
 *
 * Copyright (C) 2021 Martin Owens
 * Copyright (C) 2026 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-pages.h"

#include <glibmm/i18n.h>

#include "action-meta.h"
#include "action-registry.h"
#include "desktop.h"
#include "document-undo.h"
#include "linea-application.h"
#include "object/sp-page.h"
#include "page-manager.h"
#include "preferences.h"
#include "ui/icon-names.h"

namespace {

void page_new(SPDesktop* desktop) {
    auto document = desktop->getDocument();
    document->getPageManager().selectPage(document->getPageManager().newPage());
    Inkscape::DocumentUndo::done(document, RC_("Undo", "New Automatic Page"), INKSCAPE_ICON("tool-pages"));
}

void page_delete(SPDesktop* desktop) {
    auto document = desktop->getDocument();
    // Delete page's content if move_objects is checked.
    document->getPageManager().deletePage(document->getPageManager().move_objects());
    Inkscape::DocumentUndo::done(document, RC_("Undo", "Delete Page"), INKSCAPE_ICON("tool-pages"));
}

void page_new_and_center(SPDesktop* desktop) {
    page_new(desktop);
    desktop->getDocument()->getPageManager().centerToSelectedPage(desktop);
}

void page_delete_and_center(SPDesktop* desktop) {
    page_delete(desktop);
    desktop->getDocument()->getPageManager().centerToSelectedPage(desktop);
}

void page_backward(SPDesktop* desktop) {
    auto document = desktop->getDocument();
    auto& page_manager = document->getPageManager();
    if (auto page = page_manager.getSelected()) {
        if (page->setPageIndex(page->getPageIndex() - 1, page_manager.move_objects())) {
            Inkscape::DocumentUndo::done(document, RC_("Undo", "Shift Page Backwards"), INKSCAPE_ICON("tool-pages"));
        }
    }
}

void page_forward(SPDesktop* desktop) {
    auto document = desktop->getDocument();
    auto& page_manager = document->getPageManager();
    if (auto page = page_manager.getSelected()) {
        if (page->setPageIndex(page->getPageIndex() + 1, page_manager.move_objects())) {
            Inkscape::DocumentUndo::done(document, RC_("Undo", "Shift Page Forwards"), INKSCAPE_ICON("tool-pages"));
        }
    }
}

void toggle_move_objects(SPDesktop* desktop) {
    Inkscape::Preferences::get()->setBool("/tools/pages/move_objects", !Inkscape::PageManager::move_objects());
    desktop->getDocument()->setModifiedSinceSave();
}

bool get_move_objects(SPDesktop*) {
    return Inkscape::PageManager::move_objects();
}

const ActionGroup pageActionGroup = {"pages", N_("Page"), ActionScope::Document};

const Glib::ustring SECTION = NC_("Action Section", "Page");

static auto page_action_defs = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"page-new",           N_("New Page"),              SECTION, N_("Create a new page and center view on it"),                  "pages-add",            page_new_and_center},
    {"page-delete",        N_("Delete Page"),           SECTION, N_("Delete the selected page and center view on next page"),    "pages-remove",         page_delete_and_center},
    {"page-move-backward", N_("Move Before Previous"),  SECTION, N_("Move page backwards in the page order"),                  "pages-order-backwards", page_backward},
    {"page-move-forward",  N_("Move After Next"),       SECTION, N_("Move page forwards in the page order"),                   "pages-order-forwards",  page_forward},
    {"page-move-objects",  N_("Move Objects with Page"), SECTION, N_("Move overlapping objects as the page is moved"),          "pages-move-toggle",     toggle_move_objects, get_move_objects},
    // clang-format on
});

} // namespace

void add_actions_pages(LineaApplication* app) {
    auto& registry = ActionRegistry::get();
    registry.registerGroup(pageActionGroup);
    registry.registerActions(app, page_action_defs);
}
