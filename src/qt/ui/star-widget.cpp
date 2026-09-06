// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * StarWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#include "star-widget.h"

#include <QPushButton>

#include "actions/action-registry.h"
#include "number-edit.h"
#include "ui_star-widget.h"
#include "props/binder.h"

namespace Linea::UI {

StarWidget::StarWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::StarWidget>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

StarWidget::~StarWidget() = default;

void StarWidget::bind(Props::Binder& binder) {
    binder.bind(Props::star_sides, _ui->sidesEdit);
    binder.bind(Props::star_spoke_ratio, _ui->spokeEdit);
    binder.bind(Props::star_rounded, _ui->roundedEdit);
    binder.bind(Props::star_randomized, _ui->randomizedEdit);
    binder.bind(Props::star_flatsided, _ui->shapeToggle);

    // Turn the selected star(s) / polygon(s) upright via the registered action.
    binder.track(connect(_ui->alignButton, &QPushButton::clicked, this, [](bool) {
        auto& registry = ActionRegistry::get();
        auto action = registry.action("object-star-turn-upright");
        action->trigger();
    }));

    // reset "randomize" and "rounded" values to defaults
    binder.track(connect(_ui->resetButton, &QPushButton::clicked, this, [this]() {
        _ui->randomizedEdit->setValue(0.0);
        _ui->roundedEdit->setValue(0.0);
    }));

    // Spoke ratio only makes sense for stars; polygons disable it and require
    // at least three sides.
    binder.bindField(Props::Field::star_flatsided, [this](const Props::SelectionState& s) {
        auto& flat = s.element.star_flatsided;
        bool visible = true;
        bool polygon = true;
        if (flat.is_single()) {
            bool is_flat = flat.value() != 0;
            visible = !is_flat;
            _ui->sidesEdit->setMinimum(is_flat ? 3.0 : 2.0);
            polygon = is_flat;
        }
        _ui->spokeLabel->setVisible(visible);
        _ui->spokeEdit->setVisible(visible);
        // _ui->spokeEdit->setEnabled(!is_flat);
        _ui->alignButton->setIcon(QIcon(polygon ? ":/icons/object-level" : ":/icons/object-star-level"));
    });

    // Shown when the selection consists solely of stars or polygons.
    binder.visibleWhen(this,
        Props::Cond::allOfSum<&Props::Counts::stars, &Props::Counts::polygons>);

    // Reset button visible only when rounded or randomized is non-zero.
    binder.visibleWhen(_ui->resetButton,
        Props::Cond::differsFrom(Props::star_rounded, 0.0) ||
        Props::Cond::differsFrom(Props::star_randomized, 0.0));
}

} // namespace Linea::UI
