// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * OpacityWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "opacity-widget.h"

#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "props/binder.h"
#include "spin-scale.h"

namespace Linea::UI {

OpacityWidget::OpacityWidget(QWidget* parent)
    : QWidget(parent) {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(4);

    // Create label
    _label = new QLabel(tr("Opacity"), this);
    _label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _label->setProperty("class", "panel-label");
    _label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _layout->addWidget(_label);

    // Create SpinScale for opacity control
    _spinScale = new SpinScale(this);
    _spinScale->setRange(0.0, 100.0);
    _spinScale->setSuffix("%");
    _spinScale->setDecimals(0);
    // _spinScale->setScalingFactor(100.0);
    _spinScale->setMaxBlockCount(20);
    _spinScale->setBlockHeight(10);
    _layout->addWidget(_spinScale);

    // Prevent horizontal stretching
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

OpacityWidget::~OpacityWidget() = default;

void OpacityWidget::bind(Props::Binder& binder) {
    auto editor = binder.editor();
    if (!editor) return;

    // model -> widget (0..1 -> percentage)
    binder.bindField(Props::Field::opacity, [this](const Props::SelectionState& s) {
        auto& o = s.style.opacity;
        const QSignalBlocker blocker(_spinScale);
        if (o.is_single()) {
            _spinScale->setMixedMode(false);
            _spinScale->setValue(o.value() * 100.0);
        } else {
            _spinScale->setMixedMode(true);
        }
    });

    // widget -> model (percentage -> 0..1)
    binder.track(connect(_spinScale, &SpinScale::valueChanged, this, [editor](double v) {
        if (!editor->busy()) {
            editor->set(Props::opacity, v / 100.0);
        }
    }));

    // Shown when any items are selected.
    binder.visibleWhen(this, Props::Cond::hasSelection);
}

} // namespace Linea::UI
