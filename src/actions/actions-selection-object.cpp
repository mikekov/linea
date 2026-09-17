// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions related to manipulation a selection of objects which don't require desktop.
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/*
 * Note: Actions must be app level as different windows can have different selections
 *       and selections must also work from the command line (without GUI).
 */

#include "actions-selection-object.h"

#include <array>
#include <giomm.h>

#include "action-registry.h"
#include "actions-helper.h"
#include "document-undo.h"
#include "i18n/action-strings.h" // IWYU pragma: keep
#include "linea-application.h"
#include "page-manager.h"
#include "preferences.h"
#include "selection.h"
#include "ui/icon-names.h"

namespace {

void select_object_group(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }
    selection->group();
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Group"), INKSCAPE_ICON("object-group"));
}

void select_object_ungroup(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    selection->ungroup();
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Ungroup"), INKSCAPE_ICON("object-ungroup"));
}

void select_object_ungroup_pop(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Pop Selected Objects out of Group
    selection->popFromGroup();
}

void select_object_link(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Group with <a>
    auto anchor = selection->group(true);
    selection->set(anchor);

    // Open dialog to set link.
    // selection->desktop()->getContainer()->new_dialog("ObjectProperties");
    Inkscape::DocumentUndo::done(selection->document(), RC_("Undo", "Anchor"), INKSCAPE_ICON("object-group"));
}

void selection_top(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Raise to Top
    selection->raiseToTop();
}

void selection_raise(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Raise
    selection->raise();
}

void selection_lower(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Lower
    selection->lower();
}

void selection_bottom(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Lower to Bottom
    selection->lowerToBottom();
}

void selection_stack_up(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }
    selection->stackUp();
}

void selection_stack_down(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }
    selection->stackDown();
}

void selection_make_bitmap_copy(Inkscape::Selection* selection) {
    if (!selection) {
        return;
    }

    // Make a Bitmap Copy
    selection->createBitmapCopy();
}

void page_fit_to_selection(Inkscape::Selection* selection) {
    if (!selection) return;
    auto document = selection->document();
    if (!document) return;

    document->getPageManager().fitToSelection(selection);
    Inkscape::DocumentUndo::done(document, RC_("Undo", "Resize page to fit"), INKSCAPE_ICON("tool-pages"));
}

const Glib::ustring SECTION_SELECT = NC_("Action Section", "Select");
const Glib::ustring SECTION_PAGE = NC_("Action Section", "Page");

static auto selection_object_action_defs = std::to_array<ActionSpec<Inkscape::Selection>>({
    // clang-format off
    { "selection-group",              NC_("Verb", "Group"),                     SECTION_SELECT, N_("Group selected objects"),                                        nullptr, select_object_group},
    { "selection-ungroup",            N_("Ungroup"),                            SECTION_SELECT, N_("Ungroup selected objects"),                                      nullptr, select_object_ungroup},
    { "selection-ungroup-pop",        N_("Pop out of Group"),                   SECTION_SELECT, N_("Pop selected objects out of group"),                             nullptr, select_object_ungroup_pop},
    { "selection-link",               NC_("Hyperlink|Verb", "Link"),            SECTION_SELECT, N_("Add an anchor to selected objects"),                               nullptr, select_object_link},

    { "selection-top",                N_("Raise to Top"),                       SECTION_SELECT, N_("Raise selection to top"),                                        nullptr, selection_top},
    { "selection-raise",              N_("Raise"),                              SECTION_SELECT, N_("Raise selection one step"),                                      nullptr, selection_raise},
    { "selection-lower",              N_("Lower"),                              SECTION_SELECT, N_("Lower selection one step"),                                      nullptr, selection_lower},
    { "selection-bottom",             N_("Lower to Bottom"),                    SECTION_SELECT, N_("Lower selection to bottom"),                                     nullptr, selection_bottom},

    { "selection-stack-up",           N_("Move up the Stack"),                  SECTION_SELECT, N_("Move the selection up in the stack order"),                     nullptr, selection_stack_up},
    { "selection-stack-down",         N_("Move down the Stack"),                SECTION_SELECT, N_("Move the selection down in the stack order"),                    nullptr, selection_stack_down},

    { "selection-make-bitmap-copy",   N_("Make a Bitmap Copy"),                 SECTION_SELECT, N_("Export selection to a bitmap and insert it into document"),        nullptr, selection_make_bitmap_copy},
    { "page-fit-to-selection",        N_("Resize Page to Selection"),           SECTION_PAGE,   N_("Fit the page to the current selection or the drawing if there is no selection"), nullptr, page_fit_to_selection}
    // clang-format on
});

struct BoolActionEntry {
    BoolActionMeta meta;
    const char* pref_path;
};

static auto selection_bool_action_defs = std::to_array<BoolActionEntry>({
    // clang-format off
    {{"select-touch-box",          N_("Touch Selection"),       N_("Select objects touched by the rubber band"), "selection-touch"},
     "/tools/select/touch_box"},
    {{"select-transform-stroke",   N_("Scale Stroke Width"),    N_("When scaling objects, scale the stroke width by the same proportion"), "transform-affect-stroke"},
     "/options/transform/stroke"},
    {{"select-transform-corners",  N_("Scale Rounded Corners"), N_("When scaling rectangles, scale the radii of rounded corners"), "transform-affect-rounded-corners"},
     "/options/transform/rectcorners"},
    {{"select-transform-gradient", N_("Transform Gradients"),   N_("Move gradients (in fill or stroke) along with the objects"), "transform-affect-gradient"},
     "/options/transform/gradient"},
    {{"select-transform-pattern",  N_("Transform Patterns"),    N_("Move patterns (in fill or stroke) along with the objects"), "transform-affect-pattern"},
     "/options/transform/pattern"},
    {{"select-click-pivot",        N_("Click Handle to Set Pivot"), N_("Clicking a scale/stretch handle sets it as the pivot for keyboard scale/rotate"), "selection-click-pivot"},
     "/options/selection/click_pivot"},
    // clang-format on
});

} // namespace

void add_actions_selection_object(LineaApplication* app) {
    auto& registry = ActionRegistry::get();
    registry.registerActions(app, selection_object_action_defs);

    for (auto& e : selection_bool_action_defs) {
        auto initial = Preferences::get()->getBool(e.pref_path, false);
        auto action = registry.createBoolAction(
            e.meta, [path = e.pref_path](bool checked) { Preferences::get()->setBool(path, checked); },
            [path = e.pref_path]() { return Preferences::get()->getBool(path, false); }, initial);
        app->get_active_window()->addAction(action);
    }
}
