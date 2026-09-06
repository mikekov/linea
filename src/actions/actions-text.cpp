// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions Related to Text
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-text.h"
#include "actions-helper.h"
#include "action-registry.h"

#include <array>
#include <giomm.h>
#include "i18n/action-strings.h"

#include "linea-application.h"
#include "text-chemistry.h"

namespace {

void selection_text_put_on_path(LineaApplication* app) {
    text_put_on_path(app->get_active_desktop());
}

void selection_text_remove_from_path(LineaApplication* app) {
    text_remove_from_path(app->get_active_desktop());
}

void text_flow_into_frame(LineaApplication* app) {
    text_flow_into_shape(app->get_active_desktop());
}

void text_flow_subtract_frame(LineaApplication* app) {
    text_flow_shape_subtract(app->get_active_desktop());
}

void select_text_unflow(LineaApplication* app) {
    text_unflow(app->get_active_desktop());
}

void text_convert_to_regular(LineaApplication* app) {
    flowtext_to_text(app->get_active_desktop());
}

void text_convert_to_glyphs(LineaApplication* app) {
    text_to_glyphs(app->get_active_desktop());
}

void text_unkern(LineaApplication* app) {
    text_remove_all_kerns(app->get_active_desktop());
}

const Glib::ustring SECTION = NC_("Action Section", "Text");

static auto text_action_defs = std::to_array<ApplicationActionDef>({
    // clang-format off
    {"text-put-on-path",          N_("Put on Path"),            SECTION, N_("Put text on path"),                                                    selection_text_put_on_path},
    {"text-remove-from-path",     N_("Remove from Path"),       SECTION, N_("Remove text from path"),                                                selection_text_remove_from_path},
    {"text-flow-into-frame",     N_("Flow into Frame"),        SECTION, N_("Put text into a frame (path or shape), creating a flowed text linked to the frame object"), text_flow_into_frame},
    {"text-flow-subtract-frame",  N_("Set Subtraction Frames"), SECTION, N_("Flow text around a frame (path or shape), only available for SVG 2.0 Flow text."), text_flow_subtract_frame},
    {"text-unflow",               N_("Unflow"),                 SECTION, N_("Remove text from frame (creates a single-line text object)"),          select_text_unflow},
    {"text-convert-to-regular",   N_("Convert to Text"),        SECTION, N_("Convert flowed text to regular text object (preserves appearance)"),    text_convert_to_regular},
    {"text-convert-to-glyphs",    N_("Convert to Glyphs"),      SECTION, N_("Convert text into individual glyphs"),                                  text_convert_to_glyphs},
    {"text-unkern",               N_("Remove Manual Kerns"),    SECTION, N_("Remove all manual kerns and glyph rotations from a text object"),     text_unkern}
    // clang-format on
});

} // namespace

void add_actions_text(LineaApplication* app) {
    auto& registry = ActionRegistry::get();

    for (auto& e : text_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, app]() { fn(app); });
        app->get_active_window()->addAction(a);
    }
}
