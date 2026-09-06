// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * NodeWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#include "node-widget.h"

#include "actions/action-registry.h"
#include "desktop.h"
#include "props/binder.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/widget/custom-menu.h"

#include <array>
#include <QSignalBlocker>
#include "page-manager.h"
#include "selection.h"
#include "ui/tool/control-point-selection.h"
#include "ui/tools/node-tool.h"
#include "number-edit.h"
#include "number-range.h"
#include "ui_node-widget.h"

namespace Linea::UI {

NodeWidget::NodeWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::NodeWidget>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    _ui->nodeXEdit->setRange(NumberRange::minimum, NumberRange::maximum);
    _ui->nodeXEdit->setDecimals(NumberRange::decimals);
    _ui->nodeYEdit->setRange(NumberRange::minimum, NumberRange::maximum);
    _ui->nodeYEdit->setDecimals(NumberRange::decimals);
    _ui->distanceEdit->setRange(0, NumberRange::maximum);
    _ui->distanceEdit->setDecimals(NumberRange::decimals);

    connect(_ui->join_btn, &QPushButton::clicked, this, &NodeWidget::edit_join);
    connect(_ui->join_segment_btn, &QPushButton::clicked, this, &NodeWidget::edit_join_segment);
    connect(_ui->break_btn, &QPushButton::clicked, this, &NodeWidget::edit_break);
    connect(_ui->delete_segment_btn, &QPushButton::clicked, this, &NodeWidget::edit_delete_segment);
    connect(_ui->cusp_btn, &QPushButton::clicked, this, &NodeWidget::edit_cusp);
    connect(_ui->smooth_btn, &QPushButton::clicked, this, &NodeWidget::edit_smooth);
    connect(_ui->symmetric_btn, &QPushButton::clicked, this, &NodeWidget::edit_symmetrical);
    connect(_ui->auto_btn, &QPushButton::clicked, this, &NodeWidget::edit_auto);
    connect(_ui->line_btn, &QPushButton::clicked, this, &NodeWidget::edit_toline);
    connect(_ui->curve_btn, &QPushButton::clicked, this, &NodeWidget::edit_tocurve);

    auto& registry = ActionRegistry::get();
    connect(_ui->object_stroke_to_path_btn, &QPushButton::clicked, this,
            [&registry] { registry.action("object-stroke-to-path")->trigger(); });
    connect(_ui->object_to_path_btn, &QPushButton::clicked, this,
            [&registry] { registry.action("object-to-path")->trigger(); });

    connect(_ui->nodeXEdit, &NumberEdit::valueChanged, this,
            [this](double value) { editNodePosition(value, true); });
    connect(_ui->nodeYEdit, &NumberEdit::valueChanged, this,
            [this](double value) { editNodePosition(value, false); });
    connect(_ui->distanceEdit, &NumberEdit::valueChanged, this,
            [this](double value) { editNodeDistance(value); });

    constexpr std::array<const char*, 5> node_action_ids = {
        "node-show-outline",
        "node-show-handles",
        "node-show-transform-handles",
        "node-edit-mask",
        "node-edit-clip",
    };
    auto menu = createCustomMenu(_ui->nodeOptionsBtn, node_action_ids);
    _ui->nodeOptionsBtn->setMenu(menu);
    installRightAlignedMenuFilter(menu);
}

NodeWidget::~NodeWidget() = default;

void NodeWidget::setDesktop(SPDesktop* desktop) {
    _selectionChanged.disconnect();
    _subselectionChanged.disconnect();
    _desktop = desktop;

    if (!_desktop) {
        updateNodeControls(nullptr);
        return;
    }

    _selectionChanged = _desktop->getSelection()->connectChanged([this](auto) {
        if (auto nodeTool = getNodeTool()) {
            updateNodeControls(nodeTool->_selected_nodes);
        } else {
            updateNodeControls(nullptr);
        }
    });
    _subselectionChanged = _desktop->connect_control_point_selected(
        [this](Inkscape::UI::ControlPointSelection* selectedNodes) {
            updateNodeControls(selectedNodes);
        });

    if (auto nodeTool = getNodeTool()) {
        updateNodeControls(nodeTool->_selected_nodes);
    } else {
        updateNodeControls(nullptr);
    }
}

void NodeWidget::bind(Props::Binder& binder) {
    binder.visibleWhen(_ui->gridContainer, Props::Cond::hasSelection);
}

Inkscape::UI::Tools::NodeTool* NodeWidget::getNodeTool() const {
    if (!_desktop) {
        return nullptr;
    }
    return dynamic_cast<Inkscape::UI::Tools::NodeTool*>(_desktop->getTool());
}

void NodeWidget::updateNodeControls(Inkscape::UI::ControlPointSelection* selectedNodes) {
    _updatingNodeControls = true;
    const QSignalBlocker xBlocker(_ui->nodeXEdit);
    const QSignalBlocker yBlocker(_ui->nodeYEdit);
    const QSignalBlocker distanceBlocker(_ui->distanceEdit);

    const bool hasNodes = selectedNodes && !selectedNodes->empty();
    _ui->nodeXEdit->setEnabled(hasNodes);
    _ui->nodeYEdit->setEnabled(hasNodes);

    if (hasNodes) {
        auto midpoint = selectedNodes->pointwiseBounds()->midpoint();
        if (_desktop->getDocument()->get_origin_follows_page()) {
            midpoint *= _desktop->getDocument()->getPageManager().getSelectedPageAffine().inverse();
        }
        _ui->nodeXEdit->setValue(midpoint.x());
        _ui->nodeYEdit->setValue(midpoint.y());
    }
    else {
        _ui->nodeXEdit->setValue(0.0);
        _ui->nodeYEdit->setValue(0.0);
    }

    const bool hasDistance = hasNodes && selectedNodes->size() == 2;
    _ui->distanceLabel->setVisible(hasDistance);
    _ui->distanceEdit->setVisible(hasDistance);
    if (hasDistance) {
        _ui->distanceEdit->setValue(selectedNodes->pointwiseBounds()->diameter());
    }

    _updatingNodeControls = false;
}

void NodeWidget::editNodePosition(double value, bool xCoordinate) {
    if (_updatingNodeControls) {
        return;
    }

    auto nodeTool = getNodeTool();
    if (!nodeTool || !nodeTool->_selected_nodes || nodeTool->_selected_nodes->empty()) {
        return;
    }

    auto midpoint = nodeTool->_selected_nodes->pointwiseBounds()->midpoint();
    if (_desktop->getDocument()->get_origin_follows_page()) {
        midpoint *= _desktop->getDocument()->getPageManager().getSelectedPageAffine().inverse();
    }

    Geom::Point delta;
    delta[xCoordinate ? Geom::X : Geom::Y] = value - midpoint[xCoordinate ? Geom::X : Geom::Y];
    nodeTool->_multipath->move(delta);
}

void NodeWidget::editNodeDistance(double value) {
    if (_updatingNodeControls || value <= 0) {
        return;
    }

    auto nodeTool = getNodeTool();
    if (!nodeTool || !nodeTool->_selected_nodes || nodeTool->_selected_nodes->size() != 2) {
        return;
    }

    auto bounds = nodeTool->_selected_nodes->pointwiseBounds();
    if (!bounds || bounds->diameter() <= 0) {
        return;
    }

    auto center = nodeTool->_selected_nodes->firstSelectedPoint().value_or(bounds->midpoint());
    nodeTool->_multipath->scale(center, {value / bounds->diameter(), value / bounds->diameter()});
}

void NodeWidget::edit_join() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->joinNodes();
    }
}

void NodeWidget::edit_break() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->breakNodes();
    }
}

void NodeWidget::edit_join_segment() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->joinSegments();
    }
}

void NodeWidget::edit_delete_segment() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->deleteSegments();
    }
}

void NodeWidget::edit_cusp() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_CUSP);
    }
}

void NodeWidget::edit_smooth() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_SMOOTH);
    }
}

void NodeWidget::edit_symmetrical() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_SYMMETRIC);
    }
}

void NodeWidget::edit_auto() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_AUTO);
    }
}

void NodeWidget::edit_toline() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->setSegmentType(Inkscape::UI::SEGMENT_STRAIGHT);
    }
}

void NodeWidget::edit_tocurve() {
    if (auto nt = getNodeTool()) {
        nt->_multipath->setSegmentType(Inkscape::UI::SEGMENT_CUBIC_BEZIER);
    }
}

} // namespace Linea::UI
