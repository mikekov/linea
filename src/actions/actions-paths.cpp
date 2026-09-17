// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt Actions for Path Actions
 *
 * Copyright (C) 2021 Sushant A.A.
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-paths.h"

#include <array>
#include <iostream>
#include <giomm.h> // Not <gtkmm.h>! To eventually allow a headless version!

#include "action-registry.h"
#include "actions-helper.h"
#include "actions-tools.h"
#include "actions/action-meta.h"
#include "desktop.h"
#include "document-undo.h"
#include "i18n/action-strings.h"
#include "linea-application.h"
#include "linea-window.h"
#include "path/path-offset.h"
#include "preferences.h"
#include "selection-chemistry.h" // SelectionHelper
#include "selection.h"           // Selection
#include "ui/icon-names.h"
#include "ui/tools/booleans-builder.h"

namespace ActionsPaths {

// App-level actions

void object_path_union(Inkscape::Selection* selection) {
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathUnion();
}

void select_path_difference(Inkscape::Selection* selection) {
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathDiff();
}

void select_path_intersection(Inkscape::Selection* selection) {
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathIntersect();
}

void select_path_exclusion(Inkscape::Selection* selection) {
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathSymDiff();
}

void select_path_division(Inkscape::Selection* selection) {
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathCut();
}

void select_path_cut(Inkscape::Selection* selection) {
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathSlice();
}

void select_path_combine(Inkscape::Selection* selection) {
    selection->unlinkRecursive(true);
    selection->combine();
}

void select_path_break_apart(Inkscape::Selection* selection) {
    selection->breakApart();
}

void select_path_split(Inkscape::Selection* selection) {
    selection->breakApart(false, false);
}

void select_path_fracture(Inkscape::Selection* selection) {
    auto boolean_builder = Inkscape::BooleanBuilder(selection);
    selection->setList(boolean_builder.shape_commit(true, true));
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Fracture"), INKSCAPE_ICON("path-fracture"));
}

void select_path_flatten(Inkscape::Selection* selection) {
    selection->strokesToPaths(false, true);
    auto boolean_builder = Inkscape::BooleanBuilder(selection, true);
    selection->setList(boolean_builder.shape_commit(true, true));
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Flatten"), INKSCAPE_ICON("path-flatten"));
}

void fill_between_paths(Inkscape::Selection* selection) {
    selection->fillBetweenMany();
}

void select_path_simplify(Inkscape::Selection* selection) {
    selection->simplifyPaths();
}

// Window-level actions

void select_path_inset(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    desktop->getSelection()->removeLPESRecursive(true);
    desktop->getSelection()->unlinkRecursive(true);
    sp_selected_path_inset(desktop);
}

void select_path_offset(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    desktop->getSelection()->removeLPESRecursive(true);
    desktop->getSelection()->unlinkRecursive(true);
    sp_selected_path_offset(desktop);
}

void select_path_inset_screen(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    desktop->getSelection()->removeLPESRecursive(true);
    desktop->getSelection()->unlinkRecursive(true);
    sp_selected_path_inset_screen(desktop, 1.0);
}

void select_path_offset_screen(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    desktop->getSelection()->removeLPESRecursive(true);
    desktop->getSelection()->unlinkRecursive(true);
    sp_selected_path_offset_screen(desktop, 1.0);
}

void select_path_offset_dynamic(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    desktop->getSelection()->removeLPESRecursive(true);
    desktop->getSelection()->unlinkRecursive(true);
    sp_selected_path_create_offset_object_zero(desktop);
    set_active_tool(desktop, "Node");
}

void select_path_offset_linked(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    desktop->getSelection()->removeLPESRecursive(true);
    desktop->getSelection()->unlinkRecursive(true);
    sp_selected_path_create_updating_offset_object_zero(desktop);
    set_active_tool(desktop, "Node");
}

void select_path_reverse(SPDesktop* desktop) {
    if (!desktop) {
        return;
    }

    Inkscape::SelectionHelper::reverse(desktop);
}

void set_shape_builder_mode(int value, SPDesktop* /*desktop*/) {
    Inkscape::Preferences* pref = Inkscape::Preferences::get();
    pref->setInt("/tools/booleans/mode", value);
}

void shape_builder_mode_add(SPDesktop* desktop) {
    set_shape_builder_mode(0, desktop);
}

void shape_builder_mode_delete(SPDesktop* desktop) {
    set_shape_builder_mode(1, desktop);
}

const Glib::ustring SECTION = NC_("Action Section", "Path");

static auto path_selection_actions = std::to_array<ActionSpec<Inkscape::Selection>>({
    // clang-format off
    {"path-union",              N_("Union"),                    SECTION, N_("Create union of selected paths"), "path-union", object_path_union},
    {"path-difference",         N_("Difference"),               SECTION, N_("Create difference of selected paths (bottom minus top)"), "path-difference", select_path_difference},
    {"path-intersection",       N_("Intersection"),             SECTION, N_("Create intersection of selected paths"), "path-intersection", select_path_intersection},
    {"path-exclusion",          N_("Exclusion"),                SECTION, N_("Create exclusive OR of selected paths (those parts that belong to only one path)"), "path-exclusion", select_path_exclusion},
    {"path-division",           N_("Division"),                 SECTION, N_("Cut the bottom path into pieces"), "path-division", select_path_division},
    {"path-cut",                N_("Cut Path"),                 SECTION, N_("Cut the bottom path's stroke into pieces, removing fill"), "path-cut", select_path_cut},
    {"path-combine",            N_("Combine"),                  SECTION, N_("Combine several paths into one"), "path-combine", select_path_combine},
    {"path-break-apart",        N_("Break Apart"),              SECTION, N_("Break selected paths into subpaths"), "path-break-apart", select_path_break_apart},
    {"path-split",              N_("Split Apart"),              SECTION, N_("Split selected paths into non-overlapping sections"), "path-split", select_path_split},
    {"path-fracture",           N_("Fracture"),                 SECTION, N_("Fracture one or more overlapping objects into all possible segments"), "path-fracture", select_path_fracture},
    {"path-flatten",            NC_("Path Flatten", "Flatten"), SECTION, N_("Flatten one or more overlapping objects into their visible parts"), "path-flatten", select_path_flatten},
    {"path-fill-between-paths", N_("Fill Between Paths"),       SECTION, N_("Create a fill object using the selected paths"), "path-fill-between-paths", fill_between_paths},
    {"path-simplify",           N_("Simplify"),                 SECTION, N_("Simplify selected paths (remove extra nodes)"), "path-simplify", select_path_simplify},
    // clang-format on
});

static auto path_desktop_actions = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"path-inset",              N_("Inset"),                    SECTION, N_("Inset selected paths"), "path-inset", select_path_inset},
    {"path-outset",             N_("Offset"),                   SECTION, N_("Offset selected paths"), "path-outset", select_path_offset},
    {"path-inset-screen",       N_("Inset Screen"),             SECTION, N_("Inset selected paths by screen pixels"), "path-inset-screen", select_path_inset_screen},
    {"path-offset-screen",      N_("Offset Screen"),            SECTION, N_("Offset selected paths by screen pixels"), "path-offset-screen", select_path_offset_screen},
    {"path-offset-dynamic",     N_("Dynamic Offset"),           SECTION, N_("Create a dynamic offset object"), "path-offset-dynamic", select_path_offset_dynamic},
    {"path-offset-linked",      N_("Linked Offset"),            SECTION, N_("Create a dynamic offset object linked to the original path"), "path-offset-linked", select_path_offset_linked},
    {"path-reverse",            N_("Reverse"),                  SECTION, N_("Reverse the direction of selected paths (useful for flipping markers)"), "path-reverse", select_path_reverse},
    {"shape-builder-mode-add",  N_("Shape Builder: Add"),       SECTION, N_("Add shapes by clicking or clicking and dragging"), "shape-builder-mode-add", shape_builder_mode_add},
    {"shape-builder-mode-delete", N_("Shape Builder: Delete"),  SECTION, N_("Remove shapes by clicking or clicking and dragging"), "shape-builder-mode-delete", shape_builder_mode_delete},
    // clang-format on
});

} // namespace ActionsPaths

using namespace ActionsPaths;

void add_actions_path(LineaApplication* app) {
    auto& registry = ActionRegistry::get();

    registry.registerActions(app, path_selection_actions);
    registry.registerActions(app, path_desktop_actions);

    auto prefs = Inkscape::Preferences::get();
    bool replace = prefs->getBool("/tools/booleans/replace", true);

    auto replace_action = registry.createBoolAction(
        {"shape-builder-replace", N_("Replace Objects"), N_("Remove selected objects when shape building is completed"),
         nullptr},
        [](bool checked) { Inkscape::Preferences::get()->setBool("/tools/booleans/replace", checked); },
        []() -> bool { return Inkscape::Preferences::get()->getBool("/tools/booleans/replace", true); }, replace);
    app->get_active_window()->addAction(replace_action);
}
