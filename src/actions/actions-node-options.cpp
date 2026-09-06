// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Qt actions for node-tool options.
 *
 */

#include "actions-node-options.h"

#include <glibmm/i18n.h>

#include "action-meta.h"
#include "action-registry.h"
#include "linea-window.h"
#include "preferences.h"

using namespace Inkscape;

namespace {

const ActionGroup nodeOptionsGroup = {"node-options", N_("Node Options"), ActionScope::Window, {}};

struct NodeOptionEntry {
    BoolActionMeta meta;
    const char* pref_path;
};

const NodeOptionEntry nodeOptions[] = {
    {{"node-show-outline", N_("Show Path Outline"), N_("Show path outline (without path effects)"), "show-path-outline"},
     "/tools/nodes/show_outline"},
    {{"node-show-handles", N_("Show Bezier Handles"), N_("Show Bezier handles of selected nodes"), "show-node-handles"},
     "/tools/nodes/show_handles"},
    {{"node-show-transform-handles", N_("Show Transform Handles"), N_("Show transformation handles for selected nodes"),
      "node-transform"},
     "/tools/nodes/show_transform_handles"},
    {{"node-edit-mask", N_("Show Mask Path"), N_("Show mask(s) of selected object(s)"), "path-mask-edit"},
     "/tools/nodes/edit_masks"},
    {{"node-edit-clip", N_("Show Clip Path"), N_("Show clipping path(s) of selected object(s)"), "path-clip-edit"},
     "/tools/nodes/edit_clipping_paths"},
};

} // namespace

void add_actions_node_options(LineaWindow* win) {
    auto& registry = ActionRegistry::get();
    registry.registerGroup(nodeOptionsGroup);

    for (auto& e : nodeOptions) {
        auto initial = Preferences::get()->getBool(e.pref_path);
        auto action = registry.createBoolAction(
            e.meta, [path = e.pref_path](bool checked) { Preferences::get()->setBool(path, checked); },
            [path = e.pref_path]() { return Preferences::get()->getBool(path); }, initial);
        win->addAction(action);
    }
}
