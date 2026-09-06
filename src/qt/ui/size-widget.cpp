// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SizeWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "size-widget.h"

#include "number-edit.h"
#include "number-range.h"
#include "popup-menu.h"
#include "transform-panel.h"
#include "ui_size-widget.h"

#include "desktop.h"
#include "document.h"
#include "preferences.h"
#include "props/binder.h"
#include "props/editor.h"
#include "props/selection-state.h"
#include "selection-chemistry.h"

namespace Linea::UI {

SizeWidget::SizeWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::SizeWidget>()) {
    _ui->setupUi(this);

    // Prevent horizontal stretching
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Set CSS class property for labels
    _ui->dimensionsLabel->setProperty("class", "panel-label");
    _ui->positionLabel->setProperty("class", "panel-label");

    _ui->xEdit->setRange(NumberRange::minimum, NumberRange::maximum);
    _ui->xEdit->setDecimals(NumberRange::decimals);
    _ui->yEdit->setRange(NumberRange::minimum, NumberRange::maximum);
    _ui->yEdit->setDecimals(NumberRange::decimals);

    _ui->widthEdit->setRange(0, NumberRange::maximum);
    _ui->widthEdit->setDecimals(NumberRange::decimals);
    _ui->heightEdit->setRange(0, NumberRange::maximum);
    _ui->heightEdit->setDecimals(NumberRange::decimals);

    // _ui->xEdit->setHasFrame(true);
    connect(_ui->transform, &QPushButton::clicked, this, &SizeWidget::onTransformClicked);
}

SizeWidget::~SizeWidget() = default;

void SizeWidget::bind(Props::Binder& binder) {
    auto desktop = binder.editor()->desktop();
    if (!desktop) return;

    auto editor = binder.editor();

    // Read: refresh all four fields when the selection bounding box changes.
    // The model stores the bbox (visual or geometric per preference); we
    // convert it to display coordinates via sp_bbox_to_xywh, which applies
    // anchor adjustment, page-origin correction, and unit conversion.
    binder.bindField(Props::Field::geometry, [this, desktop](const Props::SelectionState& s) {
        auto scoped = _update.block();
        auto unit = desktop->getDocument()->getDisplayUnit();
        auto rect = sp_bbox_to_xywh(s.bbox, desktop->getSelection(), desktop, unit);
        _ui->xEdit->setValue(rect.min().x());
        _ui->yEdit->setValue(rect.min().y());
        _ui->widthEdit->setValue(rect.width());
        _ui->heightEdit->setValue(rect.height());
    });

    // Write: each edit calls sp_transform_selected_items with the full rect,
    // reading the other three values from the widgets (same pattern as
    // object-attributes.cpp's translate/transform).
    auto applyTransform = [this, desktop, editor](double x, double y, double w, double h) {
        if (_update.pending() || !desktop) return;
        auto prefs = Preferences::get();
        bool transform_stroke = prefs->getBool("/options/transform/stroke", true);
        bool preserve_transform = prefs->getBool("/options/preservetransform/value", false);
        bool use_visual = prefs->getInt("/tools/bounding_box") == 0;
        auto unit = desktop->getDocument()->getDisplayUnit();
        auto rect = Geom::Rect::from_xywh(x, y, w, h);
        sp_transform_selected_items(desktop, rect, unit, "object-properties-",
                                    transform_stroke, preserve_transform, use_visual);
    };

    binder.track(connect(_ui->xEdit, &NumberEdit::valueChanged, this,
        [this, applyTransform](double x) {
            applyTransform(x, _ui->yEdit->value(), _ui->widthEdit->value(), _ui->heightEdit->value());
        }));
    binder.track(connect(_ui->yEdit, &NumberEdit::valueChanged, this,
        [this, applyTransform](double y) {
            applyTransform(_ui->xEdit->value(), y, _ui->widthEdit->value(), _ui->heightEdit->value());
        }));
    binder.track(connect(_ui->widthEdit, &NumberEdit::valueChanged, this,
        [this, applyTransform](double w) {
            applyTransform(_ui->xEdit->value(), _ui->yEdit->value(), w, _ui->heightEdit->value());
        }));
    binder.track(connect(_ui->heightEdit, &NumberEdit::valueChanged, this,
        [this, applyTransform](double h) {
            applyTransform(_ui->xEdit->value(), _ui->yEdit->value(), _ui->widthEdit->value(), h);
        }));

    // Visible when there's a selection, but not when only a lone <svg> is selected.
    binder.visibleWhen(this, Props::Cond::hasSelection && !(Props::Cond::singleSelection && Props::Cond::allOf<&Props::Counts::svgs>));
}

void SizeWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void SizeWidget::onTransformClicked() {
    if (!_transformPopup) {
        _transformPopup = new PopupMenu(this);
        _transformPanel = new TransformPanel();
        _transformPopup->setContent(_transformPanel);

        if (_desktop) {
            _transformPanel->setDesktop(_desktop);
        }
    }

    if (_desktop) {
        auto selection = _desktop->getSelection();
        if (selection) {
            _transformPanel->updateUi(*selection);
        }
    }

    _transformPopup->showLeftOfWidget(_ui->transform);
}

} // namespace Linea::UI
