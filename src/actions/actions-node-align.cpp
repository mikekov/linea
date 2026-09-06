// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt Actions for aligning and distributing nodes. Requires Node tool.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Some code and ideas from src/ui/dialogs/align-and-distribute.cpp
 *   Authors: Bryce Harrington
 *            Martin Owens
 *            John Smith
 *            Patrick Storz
 *            Jabier Arraiza
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 * To do: Remove GUI dependency!
 */

#include "actions-node-align.h"

#include <array>
#include <iostream>
#include <limits>
#include <giomm.h> // Not <gtkmm.h>! To eventually allow a headless version!
#include <2geom/coord.h>

#include "action-registry.h"
#include "actions/action-meta.h"
#include "actions-helper.h"
#include "desktop.h"
#include "i18n/action-strings.h"
#include "linea-window.h"
#include "preferences.h"
#include "ui/tool/multi-path-manipulator.h" // Node align/distribute
#include "ui/tool/node-types.h"
#include "ui/tools/node-tool.h" // Node align/distribute

namespace {

using Inkscape::UI::AlignTargetNode;

void node_align(LineaWindow* win, Geom::Dim2 direction, Glib::ustring target_str) {
    auto dt = win->get_desktop();
    if (!dt) {
        return;
    }

    const auto tool = dt->getTool();
    auto node_tool = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(tool);
    if (!node_tool) {
        show_output("node_align: tool is not Node tool!");
        return;
    }

    if (target_str == "pref") {
        Inkscape::Preferences* prefs = Inkscape::Preferences::get();
        target_str = prefs->getString("/dialogs/align/nodes-align-to", "first");
    }

    // clang-format off
    auto target = AlignTargetNode::MID_NODE;
    if      (target_str == "last"   ) target = AlignTargetNode::LAST_NODE;
    else if (target_str == "first"  ) target = AlignTargetNode::FIRST_NODE;
    else if (target_str == "middle" ) target = AlignTargetNode::MID_NODE;
    else if (target_str == "min"    ) target = AlignTargetNode::MIN_NODE;
    else if (target_str == "max"    ) target = AlignTargetNode::MAX_NODE;
    // clang-format on
    node_tool->_multipath->alignNodes(direction, target);
}

void node_distribute(LineaWindow* win, Geom::Dim2 direction) {
    auto dt = win->get_desktop();
    if (!dt) {
        return;
    }

    const auto tool = dt->getTool();
    auto node_tool = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(tool);
    if (!node_tool) {
        show_output("node_distribute: tool is not Node tool!");
        return;
    }

    node_tool->_multipath->distributeNodes(direction);
}

void node_align_horizontal(LineaWindow* win) {
    node_align(win, Geom::X, "pref");
}
void node_align_vertical(LineaWindow* win) {
    node_align(win, Geom::Y, "pref");
}
void node_distribute_horizontal(LineaWindow* win) {
    node_distribute(win, Geom::X);
}
void node_distribute_vertical(LineaWindow* win) {
    node_distribute(win, Geom::Y);
}

const Glib::ustring SECTION = NC_("Action Section", "Node");

static auto node_align_action_defs = std::to_array<WindowActionDef>({
    // clang-format off
    {"node-align-horizontal",       N_("Align nodes horizontally"),      SECTION, N_("Align selected nodes horizontally"), node_align_horizontal},
    {"node-align-vertical",         N_("Align nodes vertically"),        SECTION, N_("Align selected nodes vertically"), node_align_vertical},
    {"node-distribute-horizontal",  N_("Distribute nodes horizontally"), SECTION, N_("Distribute selected nodes horizontally"), node_distribute_horizontal},
    {"node-distribute-vertical",    N_("Distribute nodes vertically"),   SECTION, N_("Distribute selected nodes vertically"), node_distribute_vertical}
    // clang-format on
});

} // namespace

// These are window actions as they require the node tool to be active and nodes to be selected.
void add_actions_node_align(LineaWindow* win) {
    auto& registry = ActionRegistry::get();

    for (auto& e : node_align_action_defs) {
        QAction* a = registry.createAction(e, [fn = e.callback, win]() { fn(win); });
        win->addAction(a);
    }
}
