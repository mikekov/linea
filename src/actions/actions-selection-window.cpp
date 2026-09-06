// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions related to selection which require desktop.
 *
 * These are linked to the desktop as they operate differently
 * depending on if the node tool is in use or not.
 *
 * To do: Rewrite select_same_fill_and_stroke to remove desktop dependency.
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-selection-window.h"
#include "actions-helper.h"
#include "action-registry.h"

#include <array>
#include <giomm.h>
#include "i18n/action-strings.h"

#include "desktop.h"
#include "linea-window.h"
#include "selection-chemistry.h"
// #include "ui/dialog/dialog-container.h"

#include "actions/actions-tools.h"

namespace {

void
select_all(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Select All
    Inkscape::SelectionHelper::selectAll(dt);
}

void
select_all_layers(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Select All in All Layers
    Inkscape::SelectionHelper::selectAllInAll(dt);
}

void
select_same_fill_and_stroke(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Fill and Stroke
    Inkscape::SelectionHelper::selectSameFillStroke(dt);
}

void
select_same_fill(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Fill Color
    Inkscape::SelectionHelper::selectSameFillColor(dt);
}

void
select_same_stroke_color(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Stroke Color
    Inkscape::SelectionHelper::selectSameStrokeColor(dt);
}

void
select_same_stroke_style(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Stroke Style
    Inkscape::SelectionHelper::selectSameStrokeStyle(dt);
}

void
select_same_object_type(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Object Type
    Inkscape::SelectionHelper::selectSameObjectType(dt);
}

void
select_invert(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Invert Selection
    Inkscape::SelectionHelper::invert(dt);
}

void
select_invert_all(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Invert Selection
    Inkscape::SelectionHelper::invertAllInAll(dt);
}

void
select_none(LineaWindow* win)
{
    SPDesktop* dt = win->get_desktop();

    // Deselect
    Inkscape::SelectionHelper::selectNone(dt);
}

const Glib::ustring SECTION = NC_("Action Section", "Select");

static auto select_window_action_defs = std::to_array<WindowActionDef>({
    // clang-format off
    {"select-all",                   N_("Select All"),                  SECTION, N_("Select all objects or all nodes"),                                         select_all},
    {"select-all-layers",            N_("Select All in All Layers"),    SECTION, N_("Select all objects in all visible and unlocked layers"),                    select_all_layers},
    {"select-same-fill-and-stroke",  N_("Fill and Stroke"),             SECTION, N_("Select all objects with the same fill and stroke as the selected objects"), select_same_fill_and_stroke},
    {"select-same-fill",             N_("Fill Color"),                  SECTION, N_("Select all objects with the same fill as the selected objects"),            select_same_fill},
    {"select-same-stroke-color",     N_("Stroke Color"),                SECTION, N_("Select all objects with the same stroke as the selected objects"),          select_same_stroke_color},
    {"select-same-stroke-style",     N_("Stroke Style"),                SECTION, N_("Select all objects with the same stroke style (width, dash, markers) as the selected objects"), select_same_stroke_style},
    {"select-same-object-type",      N_("Object Type"),                 SECTION, N_("Select all objects with the same object type (rect, arc, text, path, bitmap etc) as the selected objects"), select_same_object_type},
    {"select-invert",                N_("Invert Selection"),            SECTION, N_("Invert selection (unselect what is selected and select everything else)"),  select_invert},
    {"select-invert-all",            N_("Invert in All Layers"),        SECTION, N_("Invert selection in all visible and unlocked layers"),                      select_invert_all},
    {"select-none",                  N_("Deselect"),                    SECTION, N_("Deselect any selected objects or nodes"),                                  select_none}
    // clang-format on
});

} // namespace

void add_actions_select_window(LineaWindow* win) {
    auto& registry = ActionRegistry::get();

    for (auto& e : select_window_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }
}
