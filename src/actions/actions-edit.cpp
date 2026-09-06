// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Editing an object
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-edit.h"

#include <array>
#include <giomm.h>
// #include <glibmm/i18n.h>
#include "i18n/action-strings.h"

#include "actions/action-meta.h"
#include "actions-helper.h"
#include "action-registry.h"
#include "desktop.h"
#include "document-undo.h"
#include "linea-application.h"
#include "object/sp-guide.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "ui/icon-names.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/text-tool.h"

namespace ActionsEdit {

void object_to_pattern(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Objects to Pattern
    selection->tile();
}

void pattern_to_object(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Pattern to Objects
    selection->untile();
}

void object_to_marker(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Objects to Marker
    selection->toMarker();
}

void object_to_guides(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Objects to Guides
    selection->toGuides();
}

void cut(LineaApplication* app) {
    auto selection = app->get_active_selection();

    // Cut
    selection->cut();
}

void copy(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Copy
    selection->copy();
}

void paste_style(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Style
    selection->pasteStyle();
}

void paste_size(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Size
    selection->pasteSize(true, true);
}

void paste_width(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Width
    selection->pasteSize(true, false);
}

void paste_height(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Height
    selection->pasteSize(false, true);
}

void paste_size_separately(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Size Separately
    selection->pasteSizeSeparately(true, true);
}

void paste_width_separately(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Width Separately
    selection->pasteSizeSeparately(true, false);
}

void paste_height_separately(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Height Separately
    selection->pasteSizeSeparately(false, true);
}

void duplicate(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Duplicate
    selection->duplicate();
}

void duplicate_transform(LineaApplication* app) {
    auto selection = app->get_active_selection();
    selection->duplicate(true);
    selection->reapplyAffine();
    Inkscape::DocumentUndo::done(app->get_active_document(), RC_("Undo", "Duplicate and Transform"),
                                 INKSCAPE_ICON("edit-duplicate"));
}

void clone(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Create Clone
    selection->clone();
}

void clone_unlink(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Unlink Clone
    selection->unlink();
}

void clone_unlink_recursively(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Unlink Clones recursively
    selection->unlinkRecursive(false, true);
}

void clone_link(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Relink to Copied
    selection->relink();
}

void select_original(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Select Original
    selection->cloneOriginal();
}

void clone_link_lpe(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Clone original path (LPE)
    selection->cloneOriginalPathLPE();
}

void edit_delete(LineaApplication* app) {
    auto selection = app->get_active_selection();

    // For text and node too special handling.
    if (auto desktop = selection->desktop()) {
        if (const auto text_tool = dynamic_cast<Inkscape::UI::Tools::TextTool*>(desktop->getTool())) {
            text_tool->deleteSelected();
            return;
        }
        if (const auto node_tool = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(desktop->getTool())) {
            // This means we delete items is no nodes are selected.
            if (node_tool->_selected_nodes) {
                node_tool->deleteSelected();
                return;
            }
        }
    }

    //  Delete select objects only.
    selection->deleteItems();
}

void edit_delete_selection(LineaApplication* app) {
    auto selection = app->get_active_selection();
    selection->deleteItems();
}

void paste_path_effect(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Paste Path Effect
    selection->pastePathEffect();
}

void remove_path_effect(LineaApplication* app) {
    auto selection = app->get_active_selection();

    //  Remove Path Effect
    selection->removeLPE();
}

void swap_fill_and_stroke(LineaApplication* app) {
    auto selection = app->get_active_selection();

    // Swap fill and Stroke
    selection->swapFillStroke();
}

void fit_canvas_to_selection(LineaApplication* app) {
    auto selection = app->get_active_selection();

    // Fit Page to Selection
    selection->fitCanvas(true);
}

void chameleon_fill(LineaApplication* app) {
    app->get_active_selection()->chameleonFill();
}

const Glib::ustring SECTION = NC_("Action Section", "Edit");

// std::vector<std::vector<Glib::ustring>> raw_data_edit = {
static auto edit_action_defs = std::to_array<ApplicationActionDef>({
    // clang-format off
    {"object-to-pattern",        N_("Objects to Pattern"),        SECTION, N_("Convert selection to a rectangle with tiled pattern fill"), object_to_pattern},
    {"pattern-to-object",        N_("Pattern to Objects"),        SECTION, N_("Extract objects from a tiled pattern fill"), pattern_to_object},
    {"object-to-marker",         N_("Objects to Marker"),         SECTION, N_("Convert selection to a line marker"), object_to_marker},
    {"object-to-guides",         N_("Objects to Guides"),         SECTION, N_("Convert selected objects to a collection of guidelines aligned with their edges"), object_to_guides},
    {"cut",                      N_("Cut"),                       SECTION, N_("Cut selection to clipboard"), cut},
    {"copy",                     N_("Copy"),                      SECTION, N_("Copy selection to clipboard"), copy},
    {"paste-style",              N_("Paste Style"),               SECTION, N_("Apply the style of the copied object to selection"), paste_style},
    {"paste-size",               N_("Paste Size"),                SECTION, N_("Scale selection to match the size of the copied object"), paste_size},
    {"paste-width",              N_("Paste Width"),               SECTION, N_("Scale selection horizontally to match the width of the copied object"), paste_width},
    {"paste-height",             N_("Paste Height"),              SECTION, N_("Scale selection vertically to match the height of the copied object"), paste_height},
    {"paste-size-separately",    N_("Paste Size Separately"),     SECTION, N_("Scale each selected object to match the size of the copied object"), paste_size_separately},
    {"paste-width-separately",   N_("Paste Width Separately"),    SECTION, N_("Scale each selected object horizontally to match the width of the copied object"), paste_width_separately},
    {"paste-height-separately",  N_("Paste Height Separately"),   SECTION, N_("Scale each selected object vertically to match the height of the copied object"), paste_height_separately},
    {"duplicate",                N_("Duplicate"),                 SECTION, N_("Duplicate Selected Objects"), duplicate},
    {"duplicate-transform",      N_("Duplicate and Transform"),   SECTION, N_("Duplicate selected objects and reapply last transformation"), duplicate_transform},
    {"clone",                    N_("Create Clone"),              SECTION, N_("Create a clone (a copy linked to the original) of selected object"), clone},
    {"clone-unlink",             N_("Unlink Clone"),              SECTION, N_("Cut the selected clones' links to the originals, turning them into standalone objects"), clone_unlink},
    {"clone-unlink-recursively", N_("Unlink Clones recursively"), SECTION, N_("Unlink all clones in the selection, even if they are in groups."), clone_unlink_recursively},
    {"clone-link",               N_("Relink to Copied"),          SECTION, N_("Relink the selected clones to the object currently on the clipboard"), clone_link},
    {"select-original",          N_("Select Original"),           SECTION, N_("Select the object to which the selected clone is linked"), select_original},
    {"clone-link-lpe",           N_("Clone original path (LPE)"), SECTION, N_("Creates a new path, applies the Clone original LPE, and refers it to the selected path"), clone_link_lpe},
    {"delete",                   N_("Delete"),                    SECTION, N_("Delete selected items, nodes or text."), edit_delete},
    {"delete-selection",         N_("Delete Items"),              SECTION, N_("Delete selected items"), edit_delete_selection},
    {"paste-path-effect",        N_("Paste Path Effect"),         SECTION, N_("Apply the path effect of the copied object to selection"), paste_path_effect},
    {"remove-path-effect",       N_("Remove Path Effect"),        SECTION, N_("Remove any path effects from selected objects"), remove_path_effect},
    {"swap-fill-and-stroke",     N_("Swap fill and stroke"),      SECTION, N_("Swap fill and stroke of an object"), swap_fill_and_stroke},
    {"fit-canvas-to-selection",  N_("Fit Page to Selection"),     SECTION, N_("Fit the page to the current selection"), fit_canvas_to_selection},
    {"chameleon-fill",           N_("Chameleon Fill"),            SECTION, N_("Set each object's color to the average of all colors inside that shape."), chameleon_fill}
    // clang-format on
});

} // namespace ActionsEdit

using namespace ActionsEdit;

void add_actions_edit(LineaApplication* app) {
    auto& registry = ActionRegistry::get();

    for (auto& e : edit_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, app]() { fn(app); });
        app->get_active_window()->addAction(a);
    }
#if 0
    auto* gapp = app->gio_app();

    // clang-format off
    gapp->add_action( "object-to-pattern",               sigc::bind(sigc::ptr_fun(&object_to_pattern), app));
    gapp->add_action( "pattern-to-object",               sigc::bind(sigc::ptr_fun(&pattern_to_object), app));
    gapp->add_action( "object-to-marker",                sigc::bind(sigc::ptr_fun(&object_to_marker), app));
    gapp->add_action( "object-to-guides",                sigc::bind(sigc::ptr_fun(&object_to_guides), app));
    gapp->add_action( "cut",                             sigc::bind(sigc::ptr_fun(&cut), app));
    gapp->add_action( "copy",                            sigc::bind(sigc::ptr_fun(&copy), app));
    gapp->add_action( "paste-style",                     sigc::bind(sigc::ptr_fun(&paste_style), app));
    gapp->add_action( "paste-size",                      sigc::bind(sigc::ptr_fun(&paste_size), app));
    gapp->add_action( "paste-width",                     sigc::bind(sigc::ptr_fun(&paste_width), app));
    gapp->add_action( "paste-height",                    sigc::bind(sigc::ptr_fun(&paste_height), app));
    gapp->add_action( "paste-size-separately",           sigc::bind(sigc::ptr_fun(&paste_size_separately), app));
    gapp->add_action( "paste-width-separately",          sigc::bind(sigc::ptr_fun(&paste_width_separately), app));
    gapp->add_action( "paste-height-separately",         sigc::bind(sigc::ptr_fun(&paste_height_separately), app));
    gapp->add_action( "duplicate",                       sigc::bind(sigc::ptr_fun(&duplicate), app));
    gapp->add_action( "duplicate-transform",             sigc::bind(sigc::ptr_fun(&duplicate_transform), app));
    // explicit namespace reference added because NetBSD provides a conflicting clone() function in its libc headers
    gapp->add_action( "clone",                           sigc::bind(sigc::ptr_fun(&ActionsEdit::clone), app));
    gapp->add_action( "clone-unlink",                    sigc::bind(sigc::ptr_fun(&clone_unlink), app));
    gapp->add_action( "clone-unlink-recursively",        sigc::bind(sigc::ptr_fun(&clone_unlink_recursively), app));
    gapp->add_action( "clone-link",                      sigc::bind(sigc::ptr_fun(&clone_link), app));
    gapp->add_action( "select-original",                 sigc::bind(sigc::ptr_fun(&select_original), app));
    gapp->add_action( "clone-link-lpe",                  sigc::bind(sigc::ptr_fun(&clone_link_lpe), app));
    gapp->add_action( "delete",                          sigc::bind(sigc::ptr_fun(&edit_delete), app));
    gapp->add_action( "delete-selection",                sigc::bind(sigc::ptr_fun(&edit_delete_selection), app));
    gapp->add_action( "paste-path-effect",               sigc::bind(sigc::ptr_fun(&paste_path_effect), app));
    gapp->add_action( "remove-path-effect",              sigc::bind(sigc::ptr_fun(&remove_path_effect), app));
    gapp->add_action( "swap-fill-and-stroke",            sigc::bind(sigc::ptr_fun(&swap_fill_and_stroke), app));
    gapp->add_action( "fit-canvas-to-selection",         sigc::bind(sigc::ptr_fun(&fit_canvas_to_selection), app));
    gapp->add_action( "chameleon-fill",                  sigc::bind(sigc::ptr_fun(&chameleon_fill), app));
    // clang-format on

    if (!app) {
        show_output("add_actions_edit: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_edit);
#endif
}
