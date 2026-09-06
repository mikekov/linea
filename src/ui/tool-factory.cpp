// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Factory for ToolBase tree
 *
 * Authors:
 *   Markus Engel
 *
 * Copyright (C) 2013 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "tool-factory.h"

#include "ui/tools/arc-tool.h"
#include "ui/tools/box3d-tool.h"
#include "ui/tools/calligraphic-tool.h"
#include "ui/tools/connector-tool.h"
#include "ui/tools/dropper-tool.h"
#include "ui/tools/eraser-tool.h"
#include "ui/tools/flood-tool.h"
#include "ui/tools/gradient-tool.h"
#include "ui/tools/lpe-tool.h"
#include "ui/tools/measure-tool.h"
#include "ui/tools/mesh-tool.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/object-picker-tool.h"
#include "ui/tools/pages-tool.h"
#include "ui/tools/pencil-tool.h"
#include "ui/tools/rect-tool.h"
#include "ui/tools/marker-tool.h"
#include "ui/tools/select-tool.h"
#include "ui/tools/booleans-tool.h"
#include "ui/tools/spiral-tool.h"
#include "ui/tools/spray-tool.h"
#include "ui/tools/star-tool.h"
#include "ui/tools/text-tool.h"
#include "ui/tools/tweak-tool.h"
#include "ui/tools/zoom-tool.h"

using namespace Inkscape::UI::Tools;

namespace ToolFactory {

ToolBase* createObject(SPDesktop* desktop, const std::string& tool_name) {
    ToolBase* tool = nullptr;

    if (tool_name == "Ellipse")
        tool = new ArcTool(desktop);
    // else if (tool_name == "3DBox")
    //     tool = new Box3dTool(desktop);
    else if (tool_name == "Calligraphic")
        tool = new CalligraphicTool(desktop);
    else if (tool_name == "Connector")
        tool = new ConnectorTool(desktop);
    else if (tool_name == "Dropper")
        tool = new DropperTool(desktop);
    else if (tool_name == "Eraser")
        tool = new EraserTool(desktop);
    else if (tool_name == "PaintBucket")
        tool = new FloodTool(desktop);
    else if (tool_name == "Gradient")
        tool = new GradientTool(desktop);
    else if (tool_name == "LPETool")
        tool = new LpeTool(desktop);
    else if (tool_name == "Marker")
        tool = new MarkerTool(desktop);
    else if (tool_name == "Measure")
        tool = new MeasureTool(desktop);
    else if (tool_name == "Mesh")
        tool = new MeshTool(desktop);
    else if (tool_name == "Node")
        tool = new NodeTool(desktop);
    else if (tool_name == "ShapeBuilder")
        tool = new InteractiveBooleansTool(desktop);
    else if (tool_name == "Pages")
        tool = new PagesTool(desktop);
    else if (tool_name == "Pencil")
        tool = new PencilTool(desktop);
    else if (tool_name == "Pen")
        tool = new PenTool(desktop);
    else if (tool_name == "Rect")
        tool = new RectTool(desktop);
    else if (tool_name == "Select")
        tool = new SelectTool(desktop);
    else if (tool_name == "Spiral")
        tool = new SpiralTool(desktop);
    else if (tool_name == "Spray")
        tool = new SprayTool(desktop);
    else if (tool_name == "Star")
        tool = new StarTool(desktop, true);
    else if (tool_name == "Polygon")
        tool = new StarTool(desktop, false);
    else if (tool_name == "Text")
        tool = new TextTool(desktop);
    else if (tool_name == "Tweak")
        tool = new TweakTool(desktop);
    else if (tool_name == "Zoom")
        tool = new ZoomTool(desktop);
    else if (tool_name == "Picker")
        tool = new ObjectPickerTool(desktop);
    else {
        fprintf(stderr, "WARNING: unknown tool: '%s'\n", tool_name.c_str());
    // throw "stop";
        // Backup tool prevents crashes in signals that expect a tool to exist.
        tool = new SelectTool(desktop);
    }
    tool->set_name(tool_name);

    return tool;
}

} // namespace ToolFactory
