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
#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include "i18n/action-strings.h"

#include "actions/action-meta.h"
#include "actions-helper.h"
#include "actions-tools.h"
#include "action-registry.h"
#include "desktop.h"
#include "document-undo.h"
#include "linea-application.h"
#include "linea-window.h"
#include "preferences.h"
#include "selection.h"            // Selection
#include "selection-chemistry.h"  // SelectionHelper
#include "path/path-offset.h"
#include "ui/icon-names.h"
#include "ui/tools/booleans-builder.h"

namespace ActionsPaths {

// App-level actions

void object_path_union(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathUnion();
}

void select_path_difference(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathDiff();
}

void select_path_intersection(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathIntersect();
}

void select_path_exclusion(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathSymDiff();
}

void select_path_division(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathCut();
}

void select_path_cut(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->removeLPESRecursive(true);
    selection->unlinkRecursive(true);
    selection->pathSlice();
}

void select_path_combine(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->unlinkRecursive(true);
    selection->combine();
}

void select_path_break_apart(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->breakApart();
}

void select_path_split(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->breakApart(false, false);
}

void select_path_fracture(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    auto boolean_builder = Inkscape::BooleanBuilder(selection);
    selection->setList(boolean_builder.shape_commit(true, true));
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Fracture"), INKSCAPE_ICON("path-fracture"));
}

void select_path_flatten(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->strokesToPaths(false, true);
    auto boolean_builder = Inkscape::BooleanBuilder(selection, true);
    selection->setList(boolean_builder.shape_commit(true, true));
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Flatten"), INKSCAPE_ICON("path-flatten"));
}

void fill_between_paths(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->fillBetweenMany();
}

void select_path_simplify(LineaApplication* app)
{
    auto selection = app->get_active_selection();
    if (!selection) {
        return;
    }
    selection->simplifyPaths();
}

// Window-level actions

void select_path_inset(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    dt->getSelection()->removeLPESRecursive(true);
    dt->getSelection()->unlinkRecursive(true);
    sp_selected_path_inset(dt);
}

void select_path_offset(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    dt->getSelection()->removeLPESRecursive(true);
    dt->getSelection()->unlinkRecursive(true);
    sp_selected_path_offset(dt);
}

void select_path_inset_screen(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    dt->getSelection()->removeLPESRecursive(true);
    dt->getSelection()->unlinkRecursive(true);
    sp_selected_path_inset_screen(dt, 1.0);
}

void select_path_offset_screen(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    dt->getSelection()->removeLPESRecursive(true);
    dt->getSelection()->unlinkRecursive(true);
    sp_selected_path_offset_screen(dt, 1.0);
}

void select_path_offset_dynamic(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    dt->getSelection()->removeLPESRecursive(true);
    dt->getSelection()->unlinkRecursive(true);
    sp_selected_path_create_offset_object_zero(dt);
    set_active_tool(dt, "Node");
}

void select_path_offset_linked(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    dt->getSelection()->removeLPESRecursive(true);
    dt->getSelection()->unlinkRecursive(true);
    sp_selected_path_create_updating_offset_object_zero(dt);
    set_active_tool(dt, "Node");
}

void select_path_reverse(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        return;
    }

    Inkscape::SelectionHelper::reverse(dt);
}

void set_shape_builder_mode(int value, LineaWindow* /*win*/)
{
    Inkscape::Preferences* pref = Inkscape::Preferences::get();
    pref->setInt("/tools/booleans/mode", value);
}

void shape_builder_mode_add(LineaWindow* win)
{
    set_shape_builder_mode(0, win);
}

void shape_builder_mode_delete(LineaWindow* win)
{
    set_shape_builder_mode(1, win);
}

const Glib::ustring SECTION = NC_("Action Section", "Path");

static auto path_app_actions = std::to_array<ApplicationActionDef>({
    // clang-format off
    {"path-union",              N_("Union"),                    SECTION, N_("Create union of selected paths"), object_path_union},
    {"path-difference",         N_("Difference"),               SECTION, N_("Create difference of selected paths (bottom minus top)"), select_path_difference},
    {"path-intersection",       N_("Intersection"),             SECTION, N_("Create intersection of selected paths"), select_path_intersection},
    {"path-exclusion",          N_("Exclusion"),                SECTION, N_("Create exclusive OR of selected paths (those parts that belong to only one path)"), select_path_exclusion},
    {"path-division",           N_("Division"),                 SECTION, N_("Cut the bottom path into pieces"), select_path_division},
    {"path-cut",                N_("Cut Path"),                 SECTION, N_("Cut the bottom path's stroke into pieces, removing fill"), select_path_cut},
    {"path-combine",            N_("Combine"),                  SECTION, N_("Combine several paths into one"), select_path_combine},
    {"path-break-apart",        N_("Break Apart"),              SECTION, N_("Break selected paths into subpaths"), select_path_break_apart},
    {"path-split",              N_("Split Apart"),              SECTION, N_("Split selected paths into non-overlapping sections"), select_path_split},
    {"path-fracture",           N_("Fracture"),                 SECTION, N_("Fracture one or more overlapping objects into all possible segments"), select_path_fracture},
    {"path-flatten",            NC_("Path Flatten", "Flatten"), SECTION, N_("Flatten one or more overlapping objects into their visible parts"), select_path_flatten},
    {"path-fill-between-paths", N_("Fill Between Paths"),       SECTION, N_("Create a fill object using the selected paths"), fill_between_paths},
    {"path-simplify",           N_("Simplify"),                 SECTION, N_("Simplify selected paths (remove extra nodes)"), select_path_simplify},
    // clang-format on
});

static auto path_win_actions = std::to_array<WindowActionDef>({
    // clang-format off
    {"path-inset",              N_("Inset"),                    SECTION, N_("Inset selected paths"), select_path_inset},
    {"path-outset",             N_("Offset"),                   SECTION, N_("Offset selected paths"), select_path_offset},
    {"path-inset-screen",       N_("Inset Screen"),             SECTION, N_("Inset selected paths by screen pixels"), select_path_inset_screen},
    {"path-offset-screen",      N_("Offset Screen"),            SECTION, N_("Offset selected paths by screen pixels"), select_path_offset_screen},
    {"path-offset-dynamic",     N_("Dynamic Offset"),           SECTION, N_("Create a dynamic offset object"), select_path_offset_dynamic},
    {"path-offset-linked",      N_("Linked Offset"),            SECTION, N_("Create a dynamic offset object linked to the original path"), select_path_offset_linked},
    {"path-reverse",            N_("Reverse"),                  SECTION, N_("Reverse the direction of selected paths (useful for flipping markers)"), select_path_reverse},
    {"shape-builder-mode-add",  N_("Shape Builder: Add"),       SECTION, N_("Add shapes by clicking or clicking and dragging"), shape_builder_mode_add},
    {"shape-builder-mode-delete", N_("Shape Builder: Delete"),  SECTION, N_("Remove shapes by clicking or clicking and dragging"), shape_builder_mode_delete},
    // clang-format on
});

} // namespace ActionsPaths

using namespace ActionsPaths;

void add_actions_path(LineaApplication* app)
{
    auto& registry = ActionRegistry::get();

    for (auto& e : path_app_actions) {
        auto meta = e;
        meta.icon_name = meta.id;
        QAction* a = registry.createAction(meta, [fn = e.callback, app]() { fn(app); });
        app->get_active_window()->addAction(a);
    }
}

void add_actions_path(LineaWindow* win)
{
    auto& registry = ActionRegistry::get();

    for (auto& e : path_win_actions) {
        auto meta = e;
        meta.icon_name = meta.id;
        QAction* a = registry.createAction(meta, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }

    auto prefs = Inkscape::Preferences::get();
    bool replace = prefs->getBool("/tools/booleans/replace", true);

    auto* replace_action = registry.createBoolAction(
        {"shape-builder-replace", N_("Replace Objects"), N_("Remove selected objects when shape building is completed"), nullptr},
        [](bool checked) {
            Inkscape::Preferences::get()->setBool("/tools/booleans/replace", checked);
        },
        []() -> bool {
            return Inkscape::Preferences::get()->getBool("/tools/booleans/replace", true);
        },
        replace
    );
    win->addAction(replace_action);
}
