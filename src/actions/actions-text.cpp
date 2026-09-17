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

#include "desktop.h"
#include "linea-application.h"
#include "text-chemistry.h"

namespace {

void selection_text_put_on_path(SPDesktop* desktop) {
    if (!desktop) return;
    text_put_on_path(desktop);
}

void selection_text_remove_from_path(SPDesktop* desktop) {
    if (!desktop) return;
    text_remove_from_path(desktop);
}

void text_flow_into_frame(SPDesktop* desktop) {
    if (!desktop) return;
    text_flow_into_shape(desktop);
}

void text_flow_subtract_frame(SPDesktop* desktop) {
    if (!desktop) return;
    text_flow_shape_subtract(desktop);
}

void select_text_unflow(SPDesktop* desktop) {
    if (!desktop) return;
    text_unflow(desktop);
}

void text_convert_to_regular(SPDesktop* desktop) {
    if (!desktop) return;
    flowtext_to_text(desktop);
}

void text_convert_to_glyphs(SPDesktop* desktop) {
    if (!desktop) return;
    text_to_glyphs(desktop);
}

void text_unkern(SPDesktop* desktop) {
    if (!desktop) return;
    text_remove_all_kerns(desktop);
}

const Glib::ustring SECTION = NC_("Action Section", "Text");

static auto text_action_defs = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"text-put-on-path",          N_("Put on Path"),            SECTION, N_("Put text on path"), nullptr, selection_text_put_on_path},
    {"text-remove-from-path",     N_("Remove from Path"),       SECTION, N_("Remove text from path"), nullptr, selection_text_remove_from_path},
    {"text-flow-into-frame",     N_("Flow into Frame"),        SECTION, N_("Put text into a frame (path or shape), creating a flowed text linked to the frame object"), nullptr, text_flow_into_frame},
    {"text-flow-subtract-frame",  N_("Set Subtraction Frames"), SECTION, N_("Flow text around a frame (path or shape), only available for SVG 2.0 Flow text."), nullptr, text_flow_subtract_frame},
    {"text-unflow",               N_("Unflow"),                 SECTION, N_("Remove text from frame (creates a single-line text object)"), nullptr, select_text_unflow},
    {"text-convert-to-regular",   N_("Convert to Text"),        SECTION, N_("Convert flowed text to regular text object (preserves appearance)"), nullptr, text_convert_to_regular},
    {"text-convert-to-glyphs",    N_("Convert to Glyphs"),      SECTION, N_("Convert text into individual glyphs"), nullptr, text_convert_to_glyphs},
    {"text-unkern",               N_("Remove Manual Kerns"),    SECTION, N_("Remove all manual kerns and glyph rotations from a text object"), nullptr, text_unkern}
    // clang-format on
});

} // namespace

void add_actions_text(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, text_action_defs);
}
