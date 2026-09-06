// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SelectWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#include "select-widget.h"

#include <QAction>
#include <QPushButton>
#include <array>

#include "actions/action-registry.h"
#include "props/binder.h"
#include "ui/widget/custom-menu.h"
#include "ui_select-widget.h"

namespace Linea::UI {

SelectWidget::SelectWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::SelectWidget>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto& registry = ActionRegistry::get();
    auto connect_action = [this, &registry](QPushButton* button, const char* id) {
        if (auto action = registry.action(id)) {
            connect(button, &QPushButton::clicked, this, [action] { action->trigger(); });
        }
    };

    connect_action(_ui->align_horizontal_left, "object-align-left-pref");
    connect_action(_ui->align_horizontal_center, "object-align-hcenter-pref");
    connect_action(_ui->align_horizontal_right, "object-align-right-pref");
    connect_action(_ui->align_vertical_top, "object-align-top-pref");
    connect_action(_ui->align_vertical_center, "object-align-vcenter-pref");
    connect_action(_ui->align_vertical_bottom, "object-align-bottom-pref");

    connect_action(_ui->rotate_left_btn, "transform-rotate-left");
    connect_action(_ui->rotate_right_btn, "transform-rotate-right");
    connect_action(_ui->flip_horizontal_btn, "object-flip-horizontal");
    connect_action(_ui->flip_vertical_btn, "object-flip-vertical");
    connect_action(_ui->raise_one_step_btn, "selection-raise");
    connect_action(_ui->lower_one_step_btn, "selection-lower");

    constexpr std::array select_action_ids = {
        "select-touch-box",
        "select-click-pivot",
        "-",
        "select-transform-stroke",
        "select-transform-corners",
        "select-transform-gradient",
        "select-transform-pattern",
    };
    auto menu = createCustomMenu(_ui->selectOptionsBtn, select_action_ids);
    _ui->selectOptionsBtn->setMenu(menu);
    installRightAlignedMenuFilter(menu);
}

SelectWidget::~SelectWidget() = default;

void SelectWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void SelectWidget::bind(Props::Binder& binder) {
    binder.visibleWhen(_ui->gridContainer, Props::Cond::hasSelection);
}

} // namespace Linea::UI
