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

#include <array>
#include <giomm.h>

#include "action-registry.h"
#include "actions-helper.h"
#include "desktop.h"
#include "i18n/action-strings.h"
#include "linea-application.h"
#include "linea-window.h"
#include "selection-chemistry.h"
// #include "ui/dialog/dialog-container.h"

#include "actions/actions-tools.h"

namespace {

void select_all(SPDesktop* desktop) {
    // Select All
    Inkscape::SelectionHelper::selectAll(desktop);
}

void select_all_layers(SPDesktop* desktop) {
    // Select All in All Layers
    Inkscape::SelectionHelper::selectAllInAll(desktop);
}

void select_same_fill_and_stroke(SPDesktop* desktop) {
    // Fill and Stroke
    Inkscape::SelectionHelper::selectSameFillStroke(desktop);
}

void select_same_fill(SPDesktop* desktop) {
    // Fill Color
    Inkscape::SelectionHelper::selectSameFillColor(desktop);
}

void select_same_stroke_color(SPDesktop* desktop) {
    // Stroke Color
    Inkscape::SelectionHelper::selectSameStrokeColor(desktop);
}

void select_same_stroke_style(SPDesktop* desktop) {
    // Stroke Style
    Inkscape::SelectionHelper::selectSameStrokeStyle(desktop);
}

void select_same_object_type(SPDesktop* desktop) {
    // Object Type
    Inkscape::SelectionHelper::selectSameObjectType(desktop);
}

void select_invert(SPDesktop* desktop) {
    // Invert Selection
    Inkscape::SelectionHelper::invert(desktop);
}

void select_invert_all(SPDesktop* desktop) {
    // Invert Selection
    Inkscape::SelectionHelper::invertAllInAll(desktop);
}

void select_none(SPDesktop* desktop) {
    // Deselect
    Inkscape::SelectionHelper::selectNone(desktop);
}

const Glib::ustring SECTION = NC_("Action Section", "Select");

static auto select_window_action_defs = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"select-all",                  N_("Select All"),               SECTION, N_("Select all objects or all nodes"), nullptr, select_all},
    {"select-all-layers",           N_("Select All in All Layers"), SECTION, N_("Select all objects in all visible and unlocked layers"), nullptr, select_all_layers},
    {"select-same-fill-and-stroke", N_("Fill and Stroke"),          SECTION, N_("Select all objects with the same fill and stroke as the selected objects"), nullptr, select_same_fill_and_stroke},
    {"select-same-fill",            N_("Fill Color"),               SECTION, N_("Select all objects with the same fill as the selected objects"), nullptr, select_same_fill},
    {"select-same-stroke-color",    N_("Stroke Color"),             SECTION, N_("Select all objects with the same stroke as the selected objects"), nullptr, select_same_stroke_color},
    {"select-same-stroke-style",    N_("Stroke Style"),             SECTION, N_("Select all objects with the same stroke style (width, dash, markers) as the selected objects"), nullptr, select_same_stroke_style},
    {"select-same-object-type",     N_("Object Type"),              SECTION, N_("Select all objects with the same object type (rect, arc, text, path, bitmap etc) as the selected objects"), nullptr, select_same_object_type},
    {"select-invert",               N_("Invert Selection"),         SECTION, N_("Invert selection (unselect what is selected and select everything else)"), nullptr, select_invert},
    {"select-invert-all",           N_("Invert in All Layers"),     SECTION, N_("Invert selection in all visible and unlocked layers"), nullptr, select_invert_all},
    {"select-none",                 N_("Deselect"),                 SECTION, N_("Deselect any selected objects or nodes"), nullptr, select_none}
    // clang-format on
});

} // namespace

void add_actions_select_window(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, select_window_action_defs);
}
