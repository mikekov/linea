// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * EllipseWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ellipse-widget.h"

#include <QPushButton>

#include "number-edit.h"
#include "ui_ellipse-widget.h"
#include "props/binder.h"

namespace Linea::UI {

EllipseWidget::EllipseWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::EllipseWidget>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(_ui->roundButton, &QPushButton::clicked, this, &EllipseWidget::roundCxCy);
}

EllipseWidget::~EllipseWidget() = default;

void EllipseWidget::roundCxCy() {
    _ui->cxEdit->setValue(round(_ui->cxEdit->value()));
    _ui->cyEdit->setValue(round(_ui->cyEdit->value()));
}

void EllipseWidget::bind(Props::Binder& binder) {
    binder.bind(Props::ellipse_cx, _ui->cxEdit);
    binder.bind(Props::ellipse_cy, _ui->cyEdit);
    binder.bind(Props::ellipse_rx, _ui->rxEdit);
    binder.bind(Props::ellipse_ry, _ui->ryEdit);
    binder.bind(Props::ellipse_start_angle, _ui->startEdit);
    binder.bind(Props::ellipse_end_angle, _ui->endEdit);

    binder.bind(Props::ellipse_arc_mode, _ui->arcModeToggle);

    // Shown when the selection consists solely of ellipses.
    binder.visibleWhen(this, Props::Cond::allOf<&Props::Counts::ellipses>);
}

} // namespace Linea::UI
