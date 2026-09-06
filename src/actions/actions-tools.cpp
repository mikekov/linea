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

#include <QActionGroup>
#include <QObject>
#include <glibmm/i18n.h>

#include "actions-tools.h"
#include "actions-helper.h"
#include "action-meta.h"
#include "action-registry.h"
#include "desktop.h"
#include "linea-window.h"
#include "message-context.h"

#include "object/box3d.h"
#include "object/sp-ellipse.h"
#include "object/sp-flowtext.h"
#include "object/sp-offset.h"
#include "object/sp-path.h"
#include "object/sp-rect.h"
#include "object/sp-spiral.h"
#include "object/sp-star.h"
#include "object/sp-text.h"
#include "object/sp-marker.h"

#include "ui/tools/connector-tool.h"
#include "ui/tools/text-tool.h"
#include "ui/tools/tool-data.h"

namespace {

// Tool metadata with unique IDs and parameters
const ActionParamMeta toolMeta[] = {
    // action ID,            display name,              tooltip,                                   icon name,           tool ID
    {"tool-select",      N_("Selector"),       N_("Select and transform objects"),          "tool-pointer",      nullptr, "Select"},
    {"tool-node",        N_("Node"),           N_("Edit paths by nodes"),                   "tool-node-editor",  nullptr, "Node"},
    {"tool-shape-builder", N_("Shape Builder"), N_("Build shapes with the Boolean tool"),   "draw-booleans",     nullptr, "ShapeBuilder"},
    {"tool-rect",        N_("Rectangle"),      N_("Create rectangles and squares"),         "draw-rectangle",    nullptr, "Rect"},
    {"tool-ellipse",     N_("Oval"),           N_("Create circles, ellipses and arcs"),     "draw-ellipse",      nullptr, "Ellipse"},
    {"tool-star",        N_("Star"),           N_("Create stars"),                          "tool-star",         nullptr, "Star"},
    {"tool-polygon",     N_("Polygon"),        N_("Create polygons"),                       "draw-polygon-star", nullptr, "Polygon"},
    // {"tool-3dbox",       N_("3D Box"),         N_("Create 3D Boxes"),                       "tool-3dbox",        nullptr, "3DBox"},
    {"tool-spiral",      N_("Spiral"),         N_("Create spirals"),                        "draw-spiral",       nullptr, "Spiral"},
    {"tool-marker",      N_("Marker"),         N_("Edit markers"),                          "tool-marker",       nullptr, "Marker"},
    {"tool-pen",         N_("Pen"),            N_("Draw Bezier curves and straight lines"), "draw-path",         nullptr, "Pen"},
    {"tool-pencil",      N_("Pencil"),        N_("Draw freehand lines"),                   "draw-freehand",     nullptr, "Pencil"},
    {"tool-calligraphy", N_("Calligraphy"),   N_("Draw calligraphic or brush strokes"),    "draw-calligraphic", nullptr, "Calligraphic"},
    {"tool-text",        N_("Text"),          N_("Create and edit text objects"),          "draw-text",         nullptr, "Text"},
    {"tool-gradient",    N_("Gradient"),      N_("Create and edit gradients"),             "color-gradient",    nullptr, "Gradient"},
    {"tool-mesh",        N_("Mesh"),          N_("Create and edit meshes"),                "mesh-gradient",     nullptr, "Mesh"},
    {"tool-dropper",     N_("Color Picker"),  N_("Pick colors from image"),                "color-picker",      nullptr, "Dropper"},
    {"tool-paintbucket", N_("Paint Bucket"),  N_("Fill bounded areas"),                    "color-fill",        nullptr, "PaintBucket"},
    {"tool-tweak",       N_("Tweak"),         N_("Tweak objects by sculpting or painting"),"tool-tweak",        nullptr, "Tweak"},
    {"tool-spray",       N_("Spray"),         N_("Spray copies or clones of objects"),     "tool-spray",        nullptr, "Spray"},
    {"tool-eraser",      N_("Eraser"),        N_("Erase objects or paths"),                "draw-eraser",       nullptr, "Eraser"},
    {"tool-connector",   N_("Connector"),     N_("Create diagram connectors"),             "draw-connector",    nullptr, "Connector"},
    {"tool-lpe",         N_("LPE"),           N_("Do geometric constructions"),            "tool-lpe",          nullptr, "LPETool"},
    {"tool-zoom",        N_("Zoom"),          N_("Zoom in or out"),                        "zoom",              nullptr, "Zoom"},
    {"tool-measure",     N_("Measure"),       N_("Measure objects"),                       "tool-measure",      nullptr, "Measure"},
    {"tool-pages",       N_("Pages"),         N_("Create and edit document pages"),        "tool-pages",        nullptr, "Pages"},
};

const ActionMeta toggleMeta[] = {
    {"tool-toggle-select",  N_("Toggle Selector"),  N_("Toggle between Selector and last tool"), nullptr},
    {"tool-toggle-dropper", N_("Toggle Dropper"),   N_("Toggle between Dropper and last tool"), nullptr},
};

// ActionGroup registration for UI discovery
const ActionGroup toolActionGroup = {
    "tools", N_("Tools"), ActionScope::Window, {}
};

// Track last tool for toggling
Glib::ustring s_last_tool = "Select";

} // namespace

std::string get_active_tool(LineaWindow* win) {
    if (auto dt = win->get_desktop()) {
        return dt->getActiveTool();
    }
    return {};
}

std::string tool_action_id(std::string_view tool_name) {
    for (const auto& meta : toolMeta) {
        if (std::string_view(meta.param) == tool_name) {
            return meta.id;
        }
    }
    return {};
}

void tool_switch(const Glib::ustring& tool, SPDesktop* desktop);

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

void tool_switch(const Glib::ustring& tool, SPDesktop* desktop) {
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

void tool_toggle(const Glib::ustring& tool, LineaWindow* win) {
    SPDesktop* dt = win->get_desktop();
    if (!dt) {
        show_output("tool_toggle: no desktop!");
        return;
    }

    std::string current_tool = get_active_tool(win);
    Glib::ustring target_tool;

    if (current_tool == tool) {
        target_tool = s_last_tool;
    } else {
        s_last_tool = current_tool;
        target_tool = tool;
    }

    tool_switch(target_tool, dt);
}

void recreate_active_tool(SPDesktop* desktop) {
    if (!desktop) return;
    
    if (auto tool = desktop->getTool()) {
        desktop->setTool(tool->get_name());
    }
}

void set_active_tool(SPDesktop* desktop, const Glib::ustring& tool) {
    tool_switch(tool, desktop);
}

void add_actions_tools(LineaWindow* win) {
    // Register tool group for UI discovery
    ActionRegistry::get().registerGroup(toolActionGroup);

    // Create QActionGroup for exclusive selection (replaces radio_string)
    auto toolActionGroup = new QActionGroup(win);
    toolActionGroup->setExclusive(true);

    auto& registry = ActionRegistry::get();

    // Create actions for each tool
    for (const auto& meta : toolMeta) {
        auto action = registry.createBoolAction(
            meta,
            [win, tool = Glib::ustring(meta.param)](bool checked) {
                if (checked) {
                    tool_switch(tool, win->get_desktop());
                }
            },
            [win, tool = Glib::ustring(meta.param)]() -> bool {
                return get_active_tool(win) == tool;
            },
            /*initial=*/ Glib::ustring(meta.param) == "Select"
        );

        action->setActionGroup(toolActionGroup);
        win->addAction(action);
    }

    // Toggle actions (not part of radio group)
    auto toggleSelect = registry.createAction(
        toggleMeta[0],
        [win]() { tool_toggle("Select", win); }
    );
    win->addAction(toggleSelect);

    auto toggleDropper = registry.createAction(
        toggleMeta[1],
        [win]() { tool_toggle("Dropper", win); }
    );
    win->addAction(toggleDropper);

    // When the tool changes from outside the action system (e.g. double-click
    // on a shape), check the corresponding tool action. QActionGroup exclusivity
    // unchecks the previous one — both fire toggled, buttons update. No global
    // sync needed.
    if (auto dt = win->get_desktop()) {
        dt->connectEventContextChanged([](SPDesktop* desk, auto) {
            auto action_id = tool_action_id(desk->getActiveTool());
            if (!action_id.empty()) {
                if (auto action = ActionRegistry::get().action(action_id)) {
                    action->setChecked(true);
                }
            }
        });
    }
}
