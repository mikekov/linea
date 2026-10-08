// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Qt Actions for switching tools.
 *
 * Copyright (C) 2020 Tavmjong Bah
 * Copyright (C) 2026 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-tools.h"

#include <QActionGroup>
#include <QObject>
#include <array>
#include <glibmm/i18n.h>

#include "action-meta.h"
#include "action-registry.h"
#include "actions-helper.h"
#include "desktop.h"
#include "linea-window.h"
#include "object/box3d.h"
#include "object/sp-ellipse.h"
#include "object/sp-flowtext.h"
#include "object/sp-marker.h"
#include "object/sp-offset.h"
#include "object/sp-path.h"
#include "object/sp-rect.h"
#include "object/sp-spiral.h"
#include "object/sp-star.h"
#include "object/sp-text.h"
#include "selection.h"
#include "seltrans.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tool/node-types.h"
#include "ui/tools/connector-tool.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/pen-tool.h"
#include "ui/tools/select-tool.h"
#include "ui/tools/text-tool.h"
#include "ui/tools/tool-data.h"

namespace {

bool is_tool(const char* tool, SPDesktop* desktop) {
    return tool && desktop && desktop->getActiveTool() == tool;
}

bool has_selection(SPDesktop* desk) {
    return desk && desk->getSelection() && !desk->getSelection()->isEmpty();
}

bool select_transform_enabled(SPDesktop* desk) {
    return is_tool("Select", desk) && has_selection(desk);
}

bool node_enabled(SPDesktop* desk) {
    return is_tool("Node", desk);
}

bool pen_enabled(SPDesktop* desk) {
    return is_tool("Pen", desk);
}

void tool_switch(const char* tool, SPDesktop* desktop) {
    if (!desktop) return;

    const auto& tool_data = get_tool_data();
    auto tool_it = tool_data.find(tool);
    if (tool_it == tool_data.end()) {
        show_output(Glib::ustring("tool-switch: invalid tool name: ") + tool);
        return;
    }

    // Avoid switching to same tool
    if (desktop->getActiveTool() == tool) {
        return;
    }

    desktop->setTool(tool);

    if (auto new_tool = desktop->getTool()) {
        new_tool->set_last_active_tool(desktop->getActiveTool());
    }
}

// Track last tool for toggling
Glib::ustring s_last_tool = "Select";

void tool_toggle(const char* tool, SPDesktop* desk) {
    if (!desk) {
        show_output("tool_toggle: no desktop!");
        return;
    }

    auto active = desk->getActiveTool();
    Glib::ustring target_tool;

    if (active == tool) {
        target_tool = s_last_tool;
    } else {
        s_last_tool = active;
        target_tool = tool;
    }

    tool_switch(target_tool.c_str(), desk);
}

void start_stkey(SPDesktop* desk, Inkscape::SelTrans::StickyTransform type) {
    if (!desk || !is_tool("Select", desk) || !desk->getSelection() || desk->getSelection()->isEmpty()) {
        return;
    }

    auto select_tool = dynamic_cast<Inkscape::UI::Tools::SelectTool*>(desk->getTool());
    if (!select_tool || !select_tool->_seltrans) {
        return;
    }

    select_tool->_seltrans->grab_stkey(desk->point(), type);
}

void focus_first_widget(SPDesktop* desk) {
    if (!desk || !desk->getTool()) {
        return;
    }

    desk->getTool()->focus_first_widget();
}

enum class PenAction { ToLine, ToCurve, ToGuides };

void pen_action(SPDesktop* desk, PenAction action) {
    if (!desk || !is_tool("Pen", desk)) {
        return;
    }

    auto pen = dynamic_cast<Inkscape::UI::Tools::PenTool*>(desk->getTool());
    if (!pen) {
        return;
    }

    switch (action) {
        case PenAction::ToLine:
            pen->lastPointToLine();
            return;
        case PenAction::ToCurve:
            pen->lastPointToCurve();
            return;
        case PenAction::ToGuides:
            pen->selectionToGuides();
            return;
    }
}

enum class NodeAction {
    InsertNodes,
    DuplicateNodes,
    JoinNodes,
    JoinSegments,
    BreakNodes,
    DeleteSegments,
    Cusp,
    Smooth,
    AutoSmooth,
    Symmetric,
    ReverseSubpaths,
    StraightSegments,
    CurveSegments
};

void node_action(SPDesktop* desk, NodeAction action) {
    if (!desk || !is_tool("Node", desk)) {
        return;
    }

    auto node_tool = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(desk->getTool());
    if (!node_tool || !node_tool->_multipath) {
        return;
    }

    auto manipulator = node_tool->_multipath;
    switch (action) {
        case NodeAction::InsertNodes:
            manipulator->insertNodes();
            return;
        case NodeAction::DuplicateNodes:
            manipulator->duplicateNodes();
            return;
        case NodeAction::JoinNodes:
            manipulator->joinNodes();
            return;
        case NodeAction::JoinSegments:
            manipulator->joinSegments();
            return;
        case NodeAction::BreakNodes:
            manipulator->breakNodes();
            return;
        case NodeAction::DeleteSegments:
            manipulator->deleteSegments();
            return;
        case NodeAction::Cusp:
            manipulator->setNodeType(Inkscape::UI::NODE_CUSP);
            return;
        case NodeAction::Smooth:
            manipulator->setNodeType(Inkscape::UI::NODE_SMOOTH);
            return;
        case NodeAction::AutoSmooth:
            manipulator->setNodeType(Inkscape::UI::NODE_AUTO);
            return;
        case NodeAction::Symmetric:
            manipulator->setNodeType(Inkscape::UI::NODE_SYMMETRIC);
            return;
        case NodeAction::ReverseSubpaths:
            manipulator->reverseSubpaths();
            return;
        case NodeAction::StraightSegments:
            manipulator->setSegmentType(Inkscape::UI::SEGMENT_STRAIGHT);
            return;
        case NodeAction::CurveSegments:
            manipulator->setSegmentType(Inkscape::UI::SEGMENT_CUBIC_BEZIER);
            return;
    }
}

void append_node(SPDesktop* desk) {
    if (!desk || !is_tool("Node", desk)) {
        return;
    }

    auto node_tool = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(desk->getTool());
    if (!node_tool || !node_tool->_multipath) {
        return;
    }

    node_tool->_multipath->appendNode(desk->point());
}

const Glib::ustring SECTION = "Tools";
// Tool metadata with unique IDs and parameters
const auto toolMeta = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"tool-select",      N_("Selector"),       SECTION, N_("Select and transform objects"),          "tool-pointer",
        [](auto desk){ tool_switch("Select", desk); }, [](auto desk){ return is_tool("Select", desk); }},
    {"tool-node",        N_("Node"),           SECTION, N_("Edit paths by nodes"),                   "tool-node-editor",
        [](auto desk){ tool_switch("Node", desk); }, [](auto desk){ return is_tool("Node", desk); }},
    {"tool-shape-builder", N_("Shape Builder"), SECTION, N_("Build shapes with the Boolean tool"),   "draw-booleans",
        [](auto desk){ tool_switch("ShapeBuilder", desk); }, [](auto desk){ return is_tool("ShapeBuilder", desk); }},
    {"tool-rect",        N_("Rectangle"),      SECTION, N_("Create rectangles and squares"),         "draw-rectangle",
        [](auto desk){ tool_switch("Rect", desk); }, [](auto desk){ return is_tool("Rect", desk); }},
    {"tool-ellipse",     N_("Oval"),           SECTION, N_("Create circles, ellipses and arcs"),     "draw-ellipse",
        [](auto desk){ tool_switch("Ellipse", desk); }, [](auto desk){ return is_tool("Ellipse", desk); }},
    {"tool-star",        N_("Star"),           SECTION, N_("Create stars"),                          "tool-star",
        [](auto desk){ tool_switch("Star", desk); }, [](auto desk){ return is_tool("Star", desk); }},
    {"tool-polygon",     N_("Polygon"),        SECTION, N_("Create polygons"),                       "draw-polygon-star",
        [](auto desk){ tool_switch("Polygon", desk); }, [](auto desk){ return is_tool("Polygon", desk); }},
    {"tool-spiral",      N_("Spiral"),         SECTION, N_("Create spirals"),                        "draw-spiral",
        [](auto desk){ tool_switch("Spiral", desk); }, [](auto desk){ return is_tool("Spiral", desk); }},
    {"tool-marker",      N_("Marker"),         SECTION, N_("Edit markers"),                          "tool-marker",
        [](auto desk){ tool_switch("Marker", desk); }, [](auto desk){ return is_tool("Marker", desk); }},
    {"tool-pen",         N_("Pen"),            SECTION, N_("Draw Bezier curves and straight lines"), "draw-path",
        [](auto desk){ tool_switch("Pen", desk); }, [](auto desk){ return is_tool("Pen", desk); }},
    {"tool-pencil",      N_("Pencil"),         SECTION, N_("Draw freehand lines"),                   "draw-freehand",
        [](auto desk){ tool_switch("Pencil", desk); }, [](auto desk){ return is_tool("Pencil", desk); }},
    {"tool-calligraphy", N_("Calligraphy"),    SECTION, N_("Draw calligraphic or brush strokes"),    "draw-calligraphic",
        [](auto desk){ tool_switch("Calligraphic", desk); }, [](auto desk){ return is_tool("Calligraphic", desk); }},
    {"tool-text",        N_("Text"),           SECTION, N_("Create and edit text objects"),          "draw-text",
        [](auto desk){ tool_switch("Text", desk); }, [](auto desk){ return is_tool("Text", desk); }},
    {"tool-gradient",    N_("Gradient"),       SECTION, N_("Create and edit gradients"),             "color-gradient",
        [](auto desk){ tool_switch("Gradient", desk); }, [](auto desk){ return is_tool("Gradient", desk); }},
    {"tool-mesh",        N_("Mesh"),           SECTION, N_("Create and edit meshes"),                "mesh-gradient",
        [](auto desk){ tool_switch("Mesh", desk); }, [](auto desk){ return is_tool("Mesh", desk); }},
    {"tool-dropper",     N_("Color Picker"),   SECTION, N_("Pick colors from image"),                "color-picker",
        [](auto desk){ tool_switch("Dropper", desk); }, [](auto desk){ return is_tool("Dropper", desk); }},
    {"tool-paintbucket", N_("Paint Bucket"),   SECTION, N_("Fill bounded areas"),                    "color-fill",
        [](auto desk){ tool_switch("PaintBucket", desk); }, [](auto desk){ return is_tool("PaintBucket", desk); }},
    {"tool-tweak",       N_("Tweak"),          SECTION, N_("Tweak objects by sculpting or painting"),"tool-tweak",
        [](auto desk){ tool_switch("Tweak", desk); }, [](auto desk){ return is_tool("Tweak", desk); }},
    {"tool-spray",       N_("Spray"),          SECTION, N_("Spray copies or clones of objects"),     "tool-spray",
        [](auto desk){ tool_switch("Spray", desk); }, [](auto desk){ return is_tool("Spray", desk); }},
    {"tool-eraser",      N_("Eraser"),         SECTION, N_("Erase objects or paths"),                "draw-eraser",
        [](auto desk){ tool_switch("Eraser", desk); }, [](auto desk){ return is_tool("Eraser", desk); }},
    {"tool-connector",   N_("Connector"),      SECTION, N_("Create diagram connectors"),             "draw-connector",
        [](auto desk){ tool_switch("Connector", desk); }, [](auto desk){ return is_tool("Connector", desk); }},
    {"tool-lpe",         N_("LPE"),            SECTION, N_("Do geometric constructions"),            "tool-lpe",
        [](auto desk){ tool_switch("LPETool", desk); }, [](auto desk){ return is_tool("LPETool", desk); }},
    {"tool-zoom",        N_("Zoom"),           SECTION, N_("Zoom in or out"),                        "zoom",
        [](auto desk){ tool_switch("Zoom", desk); }, [](auto desk){ return is_tool("Zoom", desk); }},
    {"tool-measure",     N_("Measure"),        SECTION, N_("Measure objects"),                       "tool-measure",
        [](auto desk){ tool_switch("Measure", desk); }, [](auto desk){ return is_tool("Measure", desk); }},
    {"tool-pages",       N_("Pages"),          SECTION, N_("Create and edit document pages"),        "tool-pages",
        [](auto desk){ tool_switch("Pages", desk); }, [](auto desk){ return is_tool("Pages", desk); }},
    // clang-format on
});

const auto toggleMeta = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    {"tool-toggle-select",  N_("Toggle Selector"), SECTION, N_("Toggle between Selector and last tool"), nullptr,
        [](auto desk){ tool_toggle("Select", desk); }, [](auto desk){ return is_tool("Select", desk); }},
    {"tool-toggle-dropper", N_("Toggle Dropper"),  SECTION, N_("Toggle between Dropper and last tool"), nullptr,
        [](auto desk){ tool_toggle("Dropper", desk); }, [](auto desk){ return is_tool("Dropper", desk); }},
    // clang-format on
});

const auto toolSpecific = std::to_array<ActionSpec<SPDesktop>>({
    // clang-format off
    //   {"tool.all.quick-preview",    N_("Quick Preview"),          "Tools", N_("Preview how the document will look while the key is pressed.")     }
    // , {"tool.all.quick-zoom",       N_("Quick Zoom"),             "Tools", N_("Zoom into the selected objects while the key is pressed.")         }
    // , {"tool.all.quick-pan",        N_("Quick Pan Canvas"),       "Tools", N_("Pan the canvas with the mouse while the key is pressed.")          }
    // , {"tool.all.focus-first-widget", N_("Focus First Widget"),   "Tools", N_("Focus the first input widget in the active tool's toolbar.")       }
    {"tool-all-focus-first-widget", N_("Focus First Widget"), SECTION,
     N_("Focus the first input widget in the active tool's toolbar."), nullptr, focus_first_widget, nullptr, nullptr,
     [](SPDesktop* desk) { return desk && desk->getTool(); }},

    {"tool-sel-stkey-grab", N_("Grab/Move Objects"), SECTION,
     N_("Move the objects even if the mouse is not over the selected object."), nullptr,
     [](SPDesktop* desk) { start_stkey(desk, Inkscape::SelTrans::StickyTransform::Grab); }, nullptr, nullptr,
     select_transform_enabled},
    {"tool-sel-stkey-scale", N_("Scale Objects"), SECTION,
     N_("Resize or scale selected objects without using the scaling handles."), nullptr,
     [](SPDesktop* desk) { start_stkey(desk, Inkscape::SelTrans::StickyTransform::Scale); }, nullptr, nullptr,
     select_transform_enabled},
    {"tool-sel-stkey-rotate", N_("Rotate Objects"), SECTION,
     N_("Rotate the selected objects without using the rotate handles."), nullptr,
     [](SPDesktop* desk) { start_stkey(desk, Inkscape::SelTrans::StickyTransform::Rotate); }, nullptr, nullptr,
     select_transform_enabled},

    {"tool-pen-to-line", N_("Pen Segment To Line"), SECTION,
     N_("Convert the last pen segment to a straight line."), nullptr,
     [](SPDesktop* desk) { pen_action(desk, PenAction::ToLine); }, nullptr, nullptr,
     pen_enabled},
    {"tool-pen-to-curve", N_("Pen Segment To Curve"), SECTION,
     N_("Convert the last pen segment to a curved line."), nullptr,
     [](SPDesktop* desk) { pen_action(desk, PenAction::ToCurve); }, nullptr, nullptr,
     pen_enabled},
    {"tool-pen-to-guides", N_("Pen Segments To Guides"), SECTION,
     N_("Convert the pen shape into guides."), nullptr,
     [](SPDesktop* desk) { pen_action(desk, PenAction::ToGuides); }, nullptr, nullptr,
     pen_enabled},

    {"tool-node-append-node", N_("Append Node"), SECTION,
     N_("Append a node to the selected open-path endpoint."), nullptr, append_node, nullptr, nullptr,
     node_enabled},
    {"tool-node-insert-nodes", N_("Insert Nodes"), SECTION,
     N_("Insert nodes in the middle of selected segments."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::InsertNodes); }, nullptr, nullptr, node_enabled},
    {"tool-node-duplicate-nodes", N_("Duplicate Nodes"), SECTION,
     N_("Duplicate selected nodes."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::DuplicateNodes); }, nullptr, nullptr, node_enabled},
    {"tool-node-join-nodes", N_("Join Nodes"), SECTION,
     N_("Join selected nodes."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::JoinNodes); }, nullptr, nullptr, node_enabled},
    {"tool-node-join-segments", N_("Join Segments"), SECTION,
     N_("Join selected segments."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::JoinSegments); }, nullptr, nullptr, node_enabled},
    {"tool-node-break-nodes", N_("Break Nodes"), SECTION,
     N_("Break selected nodes."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::BreakNodes); }, nullptr, nullptr, node_enabled},
    {"tool-node-delete-segments", N_("Delete Segments"), SECTION,
     N_("Delete selected segments."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::DeleteSegments); }, nullptr, nullptr, node_enabled},
    {"tool-node-cusp", N_("Make Nodes Cusp"), SECTION,
     N_("Make selected nodes cusp nodes."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::Cusp); }, nullptr, nullptr, node_enabled},
    {"tool-node-smooth", N_("Make Nodes Smooth"), SECTION,
     N_("Make selected nodes smooth."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::Smooth); }, nullptr, nullptr, node_enabled},
    {"tool-node-auto-smooth", N_("Make Nodes Auto-Smooth"), SECTION,
     N_("Make selected nodes auto-smooth."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::AutoSmooth); }, nullptr, nullptr, node_enabled},
    {"tool-node-symmetric", N_("Make Nodes Symmetric"), SECTION,
     N_("Make selected nodes symmetric."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::Symmetric); }, nullptr, nullptr, node_enabled},
    {"tool-node-reverse-subpaths", N_("Reverse Subpaths"), SECTION,
     N_("Reverse selected subpaths."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::ReverseSubpaths); }, nullptr, nullptr, node_enabled},
    {"tool-node-straight-segments", N_("Make Segments Straight"), SECTION,
     N_("Make selected segments straight."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::StraightSegments); }, nullptr, nullptr, node_enabled},
    {"tool-node-curve-segments", N_("Make Segments Curved"), SECTION,
     N_("Make selected segments curved."), nullptr,
     [](SPDesktop* desk) { node_action(desk, NodeAction::CurveSegments); }, nullptr, nullptr, node_enabled}
    // clang-format on
});

// ActionGroup registration for UI discovery
const ActionGroup toolActionGroup = {"tools", N_("Tools"), ActionScope::Window};

} // namespace

std::string get_active_tool(LineaWindow* win) {
    if (auto dt = win->get_desktop()) {
        return dt->getActiveTool();
    }
    return {};
}

void open_tool_preferences(LineaWindow* win, const Glib::ustring& tool) {
    tool_preferences(tool, win);
}

void set_active_tool(SPDesktop* desktop, SPItem* item, const Geom::Point p) {
    if (!desktop) return;

    if (is<SPRect>(item)) {
        tool_switch("Rect", desktop);
    } else if (is<SPGenericEllipse>(item)) {
        tool_switch("Ellipse", desktop);
    } else if (is<SPStar>(item)) {
        tool_switch("Star", desktop);
    } else if (is<SPBox3D>(item)) {
        tool_switch("3DBox", desktop);
    } else if (is<SPSpiral>(item)) {
        tool_switch("Spiral", desktop);
    } else if (is<SPMarker>(item)) {
        tool_switch("Marker", desktop);
    } else if (is<SPPath>(item)) {
        if (Inkscape::UI::Tools::cc_item_is_connector(item)) {
            tool_switch("Connector", desktop);
        } else {
            tool_switch("Node", desktop);
        }
    } else if (is<SPText>(item) || is<SPFlowtext>(item)) {
        tool_switch("Text", desktop);
        SP_TEXT_CONTEXT(desktop->getTool())->placeCursorAt(item, p);
    } else if (is<SPOffset>(item)) {
        tool_switch("Node", desktop);
    }
}

void tool_preferences(const Glib::ustring& tool, LineaWindow* win) {
    const auto& tool_data = get_tool_data();
    auto tool_it = tool_data.find(tool);
    if (tool_it == tool_data.end()) {
        show_output(Glib::ustring("tool-preferences: invalid tool name: ") + tool);
        return;
    }

    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        show_output("tool-preferences: no desktop!");
        return;
    }

    auto prefs = Inkscape::Preferences::get();
    prefs->setInt("/dialogs/preferences/page", tool_it->second.pref);
    // QT TODO: Open preferences dialog to tool page
    Q_UNUSED(win)
}

void recreate_active_tool(SPDesktop* desktop) {
    if (!desktop) return;

    if (auto tool = desktop->getTool()) {
        desktop->setTool(tool->get_name());
    }
}

void set_active_tool(SPDesktop* desktop, const Glib::ustring& tool) {
    tool_switch(tool.c_str(), desktop);
}

void add_actions_tools(LineaApplication* app) {
    auto& registry = ActionRegistry::get();

    // Register tool group for UI discovery
    registry.registerGroup(toolActionGroup);

    registry.registerActions(app, toolMeta, true);

    registry.registerActions(app, toggleMeta);
    registry.registerActions(app, toolSpecific);
}
