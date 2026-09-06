// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt Actions for changing selection.
 *
 * Copyright (C) 2018 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-selection.h"

#include <array>
#include <iostream>
#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include "i18n/action-strings.h"

#include "actions-helper.h"
#include "action-registry.h"
#include "document.h"
#include "linea-application.h"
#include "selection.h"            // Selection

#include "object/sp-root.h"       // select_all: document->getRoot();
#include "object/sp-item-group.h" // select_all

namespace {

void select_clear(Glib::ustring /*param*/, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }
    selection->clear();
}

void select_by_id(Glib::ustring ids, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    auto tokens = Glib::Regex::split_simple("\\s*,\\s*", ids);
    for (auto id : tokens) {
        SPObject* object = document->getObjectById(id);
        if (object) {
            selection->add(object);
        } else {
            show_output(Glib::ustring("select_by_id: Did not find object with id: ") + id.raw());
        }
    }
}

void unselect_by_id(Glib::ustring ids, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    auto tokens = Glib::Regex::split_simple("\\s*,\\s*", ids);
    for (auto id : tokens) {
        SPObject* object = document->getObjectById(id);
        if (object) {
            selection->remove(object);
        } else {
            show_output(Glib::ustring("unselect_by_id: Did not find object with id: ") + id.raw());
        }
    }
}

void select_by_class(Glib::ustring klass, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    auto objects = document->getObjectsByClass(klass);
    selection->add(objects.begin(), objects.end());
}

void select_by_element(Glib::ustring element, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }
    auto objects = document->getObjectsByElement(element);
    selection->add(objects.begin(), objects.end());
}

void select_by_selector(Glib::ustring selector, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    auto objects = document->getObjectsBySelector(selector);
    selection->add(objects.begin(), objects.end());
}


// Helper
void
get_all_items_recursive(std::vector<SPObject *> &objects, SPObject *object, Glib::ustring &condition)
{
    for (auto &o : object->childList(false)) {
        if (is<SPItem>(o)) {
            auto group = cast<SPGroup>(o);
            if (condition == "layers") {
                if (group && group->layerMode() == SPGroup::LAYER) {
                    objects.emplace_back(o);
                    continue; // Layers cannot contain layers.
                }
            } else if (condition == "no-layers") {
                if (group && group->layerMode() == SPGroup::LAYER) {
                    // recurse one level
                } else {
                    objects.emplace_back(o);
                    continue;
                }
            } else if (condition == "groups") {
                if (group) {
                    objects.emplace_back(o);
                }
            } else if (condition == "all") {
                objects.emplace_back(o);
            } else {
                // no-groups, default
                if (!group) {
                    objects.emplace_back(o);
                    continue; // Non-groups cannot contain items.
                }
            }
            get_all_items_recursive(objects, o, condition);
        }
    }
}


/*
 * 'layers':            All layers.
 * 'groups':            All groups (including layers).
 * 'no-layers':         All top level objects in all layers (matches GUI "Select All in All Layers").
 * 'no-groups':         All objects other than groups (and layers).
 * 'all':               All objects including groups and their descendents.
 *
 * Note: GUI "Select All" requires knowledge of selected layer, which is a desktop property.
 */
void select_all(Glib::ustring condition, LineaApplication* app)
{
    if (condition != "" && condition != "layers" && condition != "no-layers" &&
        condition != "groups" && condition != "no-groups" && condition != "all") {
        show_output( "select_all: allowed options are '', 'all', 'layers', 'no-layers', 'groups', and 'no-groups'" );
        return;
    }

    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    std::vector<SPObject *> objects;
    get_all_items_recursive(objects, document->getRoot(), condition);

    selection->setList(objects);
}

void select_invert(Glib::ustring condition, LineaApplication* app)
{
    if (condition != "" && condition != "layers" && condition != "no-layers" &&
        condition != "groups" && condition != "no-groups" && condition != "all") {
        show_output( "select_all: allowed options are '', 'all', 'layers', 'no-layers', 'groups', and 'no-groups'" );
        return;
    }

    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    // Find all objects that match condition.
    std::vector<SPObject *> objects;
    get_all_items_recursive(objects, document->getRoot(), condition);

    // Get current selection.
    auto current = selection->items_vector();

    // Remove current selection from object vector.
    std::erase_if(objects, [&current] (SPObject const *x) {
        return std::find(current.begin(), current.end(), x) != current.end();
    });

    // Set selection to object vector.
    selection->setList(objects);
}

// Debug... print selected items
void select_list(Glib::ustring /*param*/, LineaApplication* app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    for (auto obj : selection->objects()) {
        std::stringstream buffer;
        buffer << *obj;
        show_output(buffer.str(), false);
    }
}

const Glib::ustring SECTION = NC_("Action Section", "Select");

using SelectionCallback = void (*)(Glib::ustring, LineaApplication*);
struct SelectionActionDef : ActionMeta2 {
    SelectionCallback callback;
    Glib::ustring default_param;
    const char* icon_name = nullptr;
};

static auto selection_action_defs = std::to_array<SelectionActionDef>({
    // clang-format off
    {"select-clear",             N_("Clear Selection"),       SECTION, N_("Clear selection"),                                              select_clear,        ""},
    // {"select",                   N_("Select"),                SECTION, N_("Select by ID (deprecated)"),                                  select_by_id,        ""},
    // {"unselect",                 N_("Deselect"),              SECTION, N_("Deselect by ID (deprecated)"),                                unselect_by_id,      ""},
    {"select-by-id",             N_("Select by ID"),          SECTION, N_("Select by ID"),                                                 select_by_id,        ""},
    {"unselect-by-id",           N_("Deselect by ID"),        SECTION, N_("Deselect by ID"),                                             unselect_by_id,      ""},
    {"select-by-class",          N_("Select by Class"),       SECTION, N_("Select by class"),                                            select_by_class,     ""},
    {"select-by-element",        N_("Select by Element"),     SECTION, N_("Select by SVG element (e.g. 'rect')"),                        select_by_element,   ""},
    {"select-by-selector",       N_("Select by Selector"),    SECTION, N_("Select by CSS selector"),                                     select_by_selector,  ""},
    {"select-all-opt",           N_("Select All Objects"),    SECTION, N_("Select all; options: 'all', 'layers', 'no-layers', 'groups', 'no-groups'"), select_all,       "all"},
    {"select-invert-opt",        N_("Invert Selection"),     SECTION, N_("Invert selection; options: 'all', 'layers', 'no-layers', 'groups', 'no-groups'"), select_invert, "all"},
    {"select-list",              N_("List Selection"),        SECTION, N_("Print a list of objects in current selection"),               select_list,          ""}
    // clang-format on
});

} // namespace

void add_actions_selection(LineaApplication* app) {
    auto& registry = ActionRegistry::get();

    for (auto& e : selection_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, param = e.default_param, app]() { fn(param, app); });
        app->get_active_window()->addAction(a);
    }
}
