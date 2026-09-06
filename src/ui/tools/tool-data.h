// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_TOOLS_TOOL_DATA_H
#define INKSCAPE_UI_TOOLS_TOOL_DATA_H

#include <map>
#include <string>
#include <string_view>

enum tools_enum // Todo: Make enum class.
{
    TOOLS_INVALID,
    TOOLS_SELECT,
    TOOLS_NODES,
    TOOLS_BOOLEANS,
    TOOLS_MARKER,
    TOOLS_TWEAK,
    TOOLS_SPRAY,
    TOOLS_SHAPES_RECT,
    TOOLS_SHAPES_3DBOX,
    TOOLS_SHAPES_ELLIPSE,
    TOOLS_SHAPES_STAR,
    TOOLS_SHAPES_POLYGON,
    TOOLS_SHAPES_SPIRAL,
    TOOLS_FREEHAND_PENCIL,
    TOOLS_FREEHAND_PEN,
    TOOLS_CALLIGRAPHIC,
    TOOLS_TEXT,
    TOOLS_GRADIENT,
    TOOLS_MESH,
    TOOLS_ZOOM,
    TOOLS_MEASURE,
    TOOLS_DROPPER,
    TOOLS_CONNECTOR,
    TOOLS_PAINTBUCKET,
    TOOLS_ERASER,
    TOOLS_LPETOOL,
    TOOLS_PAGES,
    TOOLS_PICKER
};

struct ToolData
{
    int tool = TOOLS_INVALID; // Todo: Change type to tools_enum
    int pref = 0;
    std::string pref_path;
    bool ownStyle = false; // Whether this tool has its own style (that can be used to draw new objects)
};

const std::map<std::string, ToolData, std::less<>>& get_tool_data();

const std::map<std::string, std::string>& get_tool_msg();

const std::string& pref_path_to_tool_name(std::string_view pref_path);

const ToolData* getToolData(std::string_view toolName);

#endif // INKSCAPE_UI_TOOLS_TOOL_DATA_H
