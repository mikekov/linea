// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main toolbar implementation with all primary tool buttons.
 */

#include "main-toolbar.h"
#include <array>
#include "actions/action-registry.h"
#include "paint-indicator.h"

namespace Linea::UI {

MainToolbar::MainToolbar(QWidget* parent)
    : Toolbar(parent)
{
    auto& a = ActionRegistry::get();

    _current_shape_action = a.action("tool-rect");
    _current_drawing_action = a.action("tool-pen");
    _current_extra_action = a.action("tool-zoom");
    _current_other_action = a.action("tool-eraser");

    addButton("tool-select");
    addButton("tool-node");

    const std::array drawing_tools = {
        "tool-pen",
        "tool-pencil",
        "tool-calligraphy",
    };
    addToolGroup(drawing_tools, _current_drawing_action);

    // Shape tools group (with popup menu)
    const std::array shape_tools = {
        "tool-rect",
        "tool-ellipse",
        "tool-star",
        "tool-polygon",
        "tool-spiral"
    };
    addToolGroup(shape_tools, _current_shape_action);

    addButton("tool-text");

    const std::array other_tools = {
        "tool-shape-builder",
        "-",
        "tool-dropper",
        "tool-eraser",
        "tool-tweak",
        "tool-spray",
        "tool-paintbucket",
        "-",
        "tool-gradient",
        "tool-mesh",
    };
    addToolGroup(other_tools, _current_other_action);

    const std::array extra_tools = {
        "tool-zoom",
        "tool-measure",
        "tool-connector",
    };
    addToolGroup(extra_tools, _current_extra_action);

    // addSpacer();
    _paintIndicator = new Linea::UI::PaintIndicator(this);
    _paintIndicator->setFixedSize(30, 30);
    addWidget(_paintIndicator);

    setCentered(true);
    finalizeLayout();
}

void MainToolbar::setStretch(bool stretch) {
    if (stretch) {
        setMinimumWidth(30);
        setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        finalizeLayout();
    }
}

} // namespace Linea::UI
