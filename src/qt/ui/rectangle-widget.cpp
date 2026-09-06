// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RectangleWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "rectangle-widget.h"

#include <QPushButton>

#include "ui_rectangle-widget.h"
#include "props/binder.h"

namespace Linea::UI {

RectangleWidget::RectangleWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::RectangleWidget>()) {
    _ui->setupUi(this);

    // Keep the reset button's grid cell from collapsing when hidden.
    auto sp = _ui->resetButton->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    _ui->resetButton->setSizePolicy(sp);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

RectangleWidget::~RectangleWidget() = default;

void RectangleWidget::bind(Props::Binder& binder) {
    binder.bind(Props::rect_rx, _ui->rxEdit);
    binder.bind(Props::rect_ry, _ui->ryEdit);

    // Sharp button resets both radii (edits coalesce: same undo key).
    auto editor = binder.editor();
    binder.track(connect(_ui->sharpButton, &QPushButton::clicked, this, [editor] {
        editor->set(Props::rect_rx, 0.0);
        editor->set(Props::rect_ry, 0.0);
    }));

    // Sharp button is enabled only when corners are currently rounded.
    binder.bindField(Props::Field::rect_round_corners, [this](const Props::SelectionState& s) {
        auto& rounded = s.element.rect_round_corners;
        _ui->sharpButton->setEnabled(rounded.is_mixed() || (rounded.is_single() && rounded.value()));
    });

    // Shown when the selection consists solely of rectangles.
    binder.visibleWhen(this, Props::Cond::allOf<&Props::Counts::rectangles>);
}

} // namespace Linea::UI
