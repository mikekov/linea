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
#include "linea-window.h"
#include "object/sp-page.h"
#include "page-manager.h"
#include "preferences.h"
#include "ui/icon-names.h"

namespace {

void page_new(SPDocument* document) {
    document->getPageManager().selectPage(document->getPageManager().newPage());
    Inkscape::DocumentUndo::done(document, RC_("Undo", "New Automatic Page"), INKSCAPE_ICON("tool-pages"));
}

void page_delete(SPDocument* document) {
    // Delete page's content if move_objects is checked.
    document->getPageManager().deletePage(document->getPageManager().move_objects());
    Inkscape::DocumentUndo::done(document, RC_("Undo", "Delete Page"), INKSCAPE_ICON("tool-pages"));
}

void page_new_and_center(LineaWindow* window) {
    auto document = window->get_document();
    auto desktop = window->get_desktop();
    if (!document || !desktop) return;

    page_new(document);
    document->getPageManager().centerToSelectedPage(desktop);
}

void page_delete_and_center(LineaWindow* window) {
    auto document = window->get_document();
    auto desktop = window->get_desktop();
    if (!document || !desktop) return;

    page_delete(document);
    document->getPageManager().centerToSelectedPage(desktop);
}

void page_backward(LineaWindow* window) {
    auto document = window->get_document();
    if (!document) return;

    auto& page_manager = document->getPageManager();
    if (auto page = page_manager.getSelected()) {
        if (page->setPageIndex(page->getPageIndex() - 1, page_manager.move_objects())) {
            Inkscape::DocumentUndo::done(document, RC_("Undo", "Shift Page Backwards"), INKSCAPE_ICON("tool-pages"));
        }
    }
}

void page_forward(LineaWindow* window) {
    auto document = window->get_document();
    if (!document) return;

    auto& page_manager = document->getPageManager();
    if (auto page = page_manager.getSelected()) {
        if (page->setPageIndex(page->getPageIndex() + 1, page_manager.move_objects())) {
            Inkscape::DocumentUndo::done(document, RC_("Undo", "Shift Page Forwards"), INKSCAPE_ICON("tool-pages"));
        }
    }
}

void set_move_objects(bool active, LineaWindow* window) {
    Inkscape::Preferences::get()->setBool("/tools/pages/move_objects", active);
    if (auto document = window->get_document()) {
        document->setModifiedSinceSave();
    }
}

bool get_move_objects(LineaWindow* window) {
    return Inkscape::PageManager::move_objects();
}

struct PageActionDef {
    ActionMeta meta;
    WindowCallback callback;
};

struct PageBoolActionDef {
    BoolActionMeta meta;
    void (*callback)(bool, LineaWindow*);
    bool (*state)(LineaWindow*);
};

const ActionGroup pageActionGroup = {"pages", N_("Page"), ActionScope::Document, {}};

const PageActionDef pageActions[] = {
    // clang-format off
    {{"page-new",            N_("New Page"),               N_("Create a new page and center view on it"),
        "pages-add"},           page_new_and_center},
    {{"page-delete",         N_("Delete Page"),            N_("Delete the selected page and center view on next page"),
        "pages-remove"},        page_delete_and_center},
    {{"page-move-backward",  N_("Move Before Previous"),   N_("Move page backwards in the page order"),
        "pages-order-backwards"}, page_backward},
    {{"page-move-forward",   N_("Move After Next"),        N_("Move page forwards in the page order"),
        "pages-order-forwards"},  page_forward},
    // clang-format on
};

const PageBoolActionDef pageBoolActions[] = {
    // clang-format off
    {{"page-move-objects",   N_("Move Objects with Page"), N_("Move overlapping objects as the page is moved"),        "pages-move-toggle",    nullptr}, set_move_objects, get_move_objects},
    // clang-format on
};

} // namespace

void add_actions_pages(LineaWindow* win) {
    auto& registry = ActionRegistry::get();
    registry.registerGroup(pageActionGroup);

    for (auto& e : pageActions) {
        auto a = registry.createAction(e.meta, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }

    for (auto& e : pageBoolActions) {
        auto a = registry.createBoolAction(
            e.meta, [fn = e.callback, win](bool on) { fn(on, win); }, [fn = e.state, win]() { return fn(win); },
            e.state(win));
        win->addAction(a);
    }
}
