// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ObjectTreeToolbar implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#include "object-tree-toolbar.h"

#include <QIcon>
#include <QLineEdit>
#include <QToolButton>

#include "layer-selector.h"
#include "object-tree-view.h"
#include "ui_object-tree-toolbar.h"

namespace Linea::UI {

ObjectTreeToolbar::ObjectTreeToolbar(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ObjectTreeToolbar>()) {
    _ui->setupUi(this);

    _ui->toolbar->setDefaultButtonSize(24);
    _ui->toolbar->setMargins(0, 0, 0, 0);
    static const auto actions = std::to_array<const char*>({
        "layer-new-above",
        "layer-new-below",
        "layer-new-child",
        "-",
        "layer-duplicate",
        // "layer-delete",
        // "layer-rename",
    });
    _ui->toolbar->addSplitMenuButton(actions, "layer-new");
    _ui->toolbar->addButton("layer-raise");
    _ui->toolbar->addButton("layer-lower");

    // Action buttons emit signals for the client to handle
    // connect(_ui->moveUpButton, &QToolButton::clicked, this, &ObjectTreeToolbar::moveUpRequested);
    // connect(_ui->moveDownButton, &QToolButton::clicked, this, &ObjectTreeToolbar::moveDownRequested);
}

ObjectTreeToolbar::~ObjectTreeToolbar() = default;

LayerSelector* ObjectTreeToolbar::getLayerSelector() {
    return _ui->layerSelector;
}

void ObjectTreeToolbar::connectToView(ObjectTreeView* view) {
    _view = view;
}

} // namespace Linea::UI
