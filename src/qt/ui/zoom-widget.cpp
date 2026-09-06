// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ZoomWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#include "zoom-widget.h"

#include "actions/action-registry.h"
#include "ui_zoom-widget.h"

namespace Linea::UI {

ZoomWidget::ZoomWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ZoomWidget>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto trigger = [](const char* id) {
        ActionRegistry::get().action(id)->trigger();
    };

    connect(_ui->zoom_in_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-in"); });
    connect(_ui->zoom_out_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-out"); });
    connect(_ui->zoom_1_1_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-1-1"); });
    connect(_ui->zoom_1_2_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-1-2"); });
    connect(_ui->zoom_2_1_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-2-1"); });
    connect(_ui->zoom_fit_selection_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-selection"); });
    connect(_ui->zoom_fit_drawing_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-drawing"); });
    connect(_ui->zoom_fit_page_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-page"); });
    connect(_ui->zoom_fit_width_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-page-width"); });
    connect(_ui->zoom_center_page_btn, &QPushButton::clicked, this, [trigger] { trigger("canvas-zoom-center-page"); });
}

ZoomWidget::~ZoomWidget() = default;

void ZoomWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

} // namespace Linea::UI
