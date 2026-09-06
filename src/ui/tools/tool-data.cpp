// SPDX-License-Identifier: GPL-2.0-or-later
#include "tool-data.h"

#include <glibmm/i18n.h>
// #include "ui/dialog/inkscape-preferences.h"

enum {
    PREFS_PAGE_TOOLS,
    PREFS_PAGE_TOOLS_SELECTOR,
    PREFS_PAGE_TOOLS_NODE,
    PREFS_PAGE_TOOLS_TWEAK,
    PREFS_PAGE_TOOLS_ZOOM,
    PREFS_PAGE_TOOLS_MEASURE,
    PREFS_PAGE_TOOLS_SHAPES,
    PREFS_PAGE_TOOLS_SHAPES_RECT,
    PREFS_PAGE_TOOLS_SHAPES_3DBOX,
    PREFS_PAGE_TOOLS_SHAPES_ELLIPSE,
    PREFS_PAGE_TOOLS_SHAPES_STAR,
    PREFS_PAGE_TOOLS_SHAPES_POLYGON,
    PREFS_PAGE_TOOLS_SHAPES_SPIRAL,
    PREFS_PAGE_TOOLS_PENCIL,
    PREFS_PAGE_TOOLS_PEN,
    PREFS_PAGE_TOOLS_CALLIGRAPHY,
    PREFS_PAGE_TOOLS_TEXT,
    PREFS_PAGE_TOOLS_SPRAY,
    PREFS_PAGE_TOOLS_ERASER,
    PREFS_PAGE_TOOLS_PAINTBUCKET,
    PREFS_PAGE_TOOLS_GRADIENT,
    PREFS_PAGE_TOOLS_DROPPER,
    PREFS_PAGE_TOOLS_CONNECTOR,
    PREFS_PAGE_TOOLS_LPETOOL,
    PREFS_PAGE_UI,
    PREFS_PAGE_UI_THEME,
    PREFS_PAGE_UI_TOOLBARS,
    PREFS_PAGE_UI_WINDOWS,
    PREFS_PAGE_UI_COLOR_PICKERS,
    PREFS_PAGE_UI_GRIDS,
    PREFS_PAGE_COMMAND_PALETTE,
    PREFS_PAGE_UI_KEYBOARD_SHORTCUTS,
    PREFS_PAGE_BEHAVIOR,
    PREFS_PAGE_BEHAVIOR_SELECTING,
    PREFS_PAGE_BEHAVIOR_TRANSFORMS,
    PREFS_PAGE_BEHAVIOR_SCROLLING,
    PREFS_PAGE_BEHAVIOR_SNAPPING,
    PREFS_PAGE_BEHAVIOR_STEPS,
    PREFS_PAGE_BEHAVIOR_CLONES,
    PREFS_PAGE_BEHAVIOR_MASKS,
    PREFS_PAGE_BEHAVIOR_MARKERS,
    PREFS_PAGE_BEHAVIOR_CLIPBOARD,
    PREFS_PAGE_BEHAVIOR_CLEANUP,
    PREFS_PAGE_BEHAVIOR_LPE,
    PREFS_PAGE_IO,
    PREFS_PAGE_IO_MOUSE,
    PREFS_PAGE_IO_SVGOUTPUT,
    PREFS_PAGE_IO_SVGEXPORT,
    PREFS_PAGE_IO_CMS,
    PREFS_PAGE_IO_AUTOSAVE,
    PREFS_PAGE_IO_OPENCLIPART,
    PREFS_PAGE_SYSTEM,
    PREFS_PAGE_BITMAPS,
    PREFS_PAGE_RENDERING,
    PREFS_PAGE_SPELLCHECK,
    PREFS_PAGE_NOTFOUND
};

const std::map<std::string, ToolData, std::less<>>& get_tool_data() {
    static std::map<std::string, ToolData, std::less<>> tool_data = {
        // clang-format off
        {"Select",       {TOOLS_SELECT,          PREFS_PAGE_TOOLS_SELECTOR,       "/tools/select",          false}},
        {"Node",         {TOOLS_NODES,           PREFS_PAGE_TOOLS_NODE,           "/tools/nodes",           false}},
        {"ShapeBuilder", {TOOLS_BOOLEANS,        PREFS_PAGE_TOOLS,/*No Page*/     "/tools/booleans",        false}},
        {"Marker",       {TOOLS_MARKER,          PREFS_PAGE_TOOLS,/*No Page*/     "/tools/marker",          false}},
        {"Rect",         {TOOLS_SHAPES_RECT,     PREFS_PAGE_TOOLS_SHAPES_RECT,    "/tools/shapes/rect",     true }},
        {"Ellipse",      {TOOLS_SHAPES_ELLIPSE,  PREFS_PAGE_TOOLS_SHAPES_ELLIPSE, "/tools/shapes/ellipse",  true }},
        {"Star",         {TOOLS_SHAPES_STAR,     PREFS_PAGE_TOOLS_SHAPES_STAR,    "/tools/shapes/star",     true }},
        {"Polygon",      {TOOLS_SHAPES_POLYGON,  PREFS_PAGE_TOOLS_SHAPES_POLYGON, "/tools/shapes/polygon",  true }},
        {"3DBox",        {TOOLS_SHAPES_3DBOX,    PREFS_PAGE_TOOLS_SHAPES_3DBOX,   "/tools/shapes/3dbox",    true }},
        {"Spiral",       {TOOLS_SHAPES_SPIRAL,   PREFS_PAGE_TOOLS_SHAPES_SPIRAL,  "/tools/shapes/spiral",   true }},
        {"Pencil",       {TOOLS_FREEHAND_PENCIL, PREFS_PAGE_TOOLS_PENCIL,         "/tools/freehand/pencil", true }},
        {"Pen",          {TOOLS_FREEHAND_PEN,    PREFS_PAGE_TOOLS_PEN,            "/tools/freehand/pen",    true }},
        {"Calligraphic", {TOOLS_CALLIGRAPHIC,    PREFS_PAGE_TOOLS_CALLIGRAPHY,    "/tools/calligraphic",    true }},
        {"Text",         {TOOLS_TEXT,            PREFS_PAGE_TOOLS_TEXT,           "/tools/text",            true }},
        {"Gradient",     {TOOLS_GRADIENT,        PREFS_PAGE_TOOLS_GRADIENT,       "/tools/gradient",        true }},
        {"Mesh",         {TOOLS_MESH,            PREFS_PAGE_TOOLS, /* No Page */  "/tools/mesh",            false}},
        {"Zoom",         {TOOLS_ZOOM,            PREFS_PAGE_TOOLS_ZOOM,           "/tools/zoom",            false}},
        {"Measure",      {TOOLS_MEASURE,         PREFS_PAGE_TOOLS_MEASURE,        "/tools/measure",         false}},
        {"Dropper",      {TOOLS_DROPPER,         PREFS_PAGE_TOOLS_DROPPER,        "/tools/dropper",         false}},
        {"Tweak",        {TOOLS_TWEAK,           PREFS_PAGE_TOOLS_TWEAK,          "/tools/tweak",           false}},
        {"Spray",        {TOOLS_SPRAY,           PREFS_PAGE_TOOLS_SPRAY,          "/tools/spray",           false}},
        {"Connector",    {TOOLS_CONNECTOR,       PREFS_PAGE_TOOLS_CONNECTOR,      "/tools/connector",       true }},
        {"PaintBucket",  {TOOLS_PAINTBUCKET,     PREFS_PAGE_TOOLS_PAINTBUCKET,    "/tools/paintbucket",     false}},
        {"Eraser",       {TOOLS_ERASER,          PREFS_PAGE_TOOLS_ERASER,         "/tools/eraser",          false}},
        {"LPETool",      {TOOLS_LPETOOL,         PREFS_PAGE_TOOLS, /* No Page */  "/tools/lpetool",         false}},
        {"Pages",        {TOOLS_PAGES,           PREFS_PAGE_TOOLS,                "/tools/pages",           false}},
        {"Picker",       {TOOLS_PICKER,          PREFS_PAGE_TOOLS, /* No Page */  "/tools/picker",          false}}
        // clang-format on
    };
    return tool_data;
}

const std::map<std::string, std::string>& get_tool_msg() {

    static std::map<std::string, std::string> tool_msg = {
        // clang-format off
        {"Select",       _("<b>Click</b> to Select and Transform objects, <b>Drag</b> to select many objects.")                                                                                                                   },
        {"Node",         _("Modify selected path points (nodes) directly.")                                                                                                                                                       },
        {"Booleans",     _("Construct shapes with the interactive Boolean tool.")                                                                                                                                                 },
        {"Rect",         _("<b>Drag</b> to create a rectangle. <b>Drag controls</b> to round corners and resize. <b>Click</b> to select.")                                                                                        },
        {"Arc",          _("<b>Drag</b> to create an ellipse. <b>Drag controls</b> to make an arc or segment. <b>Click</b> to select.")                                                                                           },
        {"Star",         _("<b>Drag</b> to create a star. <b>Drag controls</b> to edit the star shape. <b>Click</b> to select.")                                                                                                  },
        {"3DBox",        _("<b>Drag</b> to create a 3D box. <b>Drag controls</b> to resize in perspective. <b>Click</b> to select (with <b>Ctrl+Alt</b> for single faces).")                                                      },
        {"Spiral",       _("<b>Drag</b> to create a spiral. <b>Drag controls</b> to edit the spiral shape. <b>Click</b> to select.")                                                                                              },
        {"Marker",       _("<b>Click</b> a shape to start editing its markers. <b>Drag controls</b> to change orientation, scale, and position.")                                                                                 },
        {"Pencil",       _("<b>Drag</b> to create a freehand line. <b>Shift</b> appends to selected path, <b>Alt</b> activates sketch mode.")                                                                                     },
        {"Pen",          _("<b>Click</b> or <b>click and drag</b> to start a path; with <b>Shift</b> to append to selected path. <b>Ctrl+click</b> to create single dots (straight line modes only).")                            },
        {"Calligraphic", _("<b>Drag</b> to draw a calligraphic stroke; with <b>Ctrl</b> to track a guide path. <b>Arrow keys</b> adjust width (left/right) and angle (up/down).")                                                 },
        {"Text",         _("<b>Click</b> to select or create text, <b>drag</b> to create flowed text; then type.")                                                                                                                },
        {"Gradient",     _("<b>Drag</b> or <b>double click</b> to create a gradient on selected objects, <b>drag handles</b> to adjust gradients.")                                                                               },
        {"Mesh",         _("<b>Drag</b> or <b>double click</b> to create a mesh on selected objects, <b>drag handles</b> to adjust meshes.")                                                                                      },
        {"Zoom",         _("<b>Click</b> or <b>drag around an area</b> to zoom in, <b>Shift+click</b> to zoom out.")                                                                                                              },
        {"Measure",      _("<b>Drag</b> to measure the dimensions of objects.  Press <b>Alt+C</b> to copy the length to the clipboard.")                                                                                          },
        {"Dropper",      _("<b>Click</b> to set fill, <b>Shift+click</b> to set stroke; <b>drag</b> to average color in area; with <b>Alt</b> to pick inverse color; <b>Ctrl+C</b> to copy the color under mouse to clipboard")   },
        {"Tweak",        _("To tweak a path by pushing, select it and drag over it.")                                                                                                                                             },
        {"Spray",        _("<b>Drag</b>, <b>click</b> or <b>click and scroll</b> to spray the selected objects.")                                                                                                                 },
        {"Connector",    _("<b>Click and drag</b> between shapes to create a connector.")                                                                                                                                         },
        {"PaintBucket",  _("<b>Click</b> to paint a bounded area, <b>Shift+click</b> to union the new fill with the current selection, <b>Ctrl+click</b> to change the clicked object's fill and stroke to the current setting.") },
        {"Eraser",       _("<b>Drag</b> to erase.")                                                                                                                                                                               },
        {"LPETool",      _("Choose a subtool from the toolbar")                                                                                                                                                                   },
        {"Pages",        _("Create and manage pages.")},
        {"Picker",       _("Pick objects.")}
        // clang-format on
    };
    return tool_msg;
}

const std::string& pref_path_to_tool_name(std::string_view pref_path)
{
    auto &data = get_tool_data();

    // Todo: Would prefer to identify tools by their enum, not by their name
    // or prefs path. Then none of this function would even be necessary.
    for (auto &[name, data] : data) {
        if (data.pref_path == pref_path) {
            return name;
        }
    }

    static std::string const empty;
    return empty;
}

const ToolData* getToolData(std::string_view toolName) {
    auto& data = get_tool_data();
    auto it = data.find(toolName);
    if (it != data.end()) {
        return &it->second;
    }
    return nullptr;
}
