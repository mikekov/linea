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

#include <cmath>

#include "linked-number-pair.h"
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

    _ui->xEdit->setRange(NumberRange::minimum, NumberRange::maximum);
    _ui->xEdit->setDecimals(NumberRange::decimals);
    _ui->yEdit->setRange(NumberRange::minimum, NumberRange::maximum);
    _ui->yEdit->setDecimals(NumberRange::decimals);

    _ui->sizePair->first()->setRange(0, NumberRange::maximum);
    _ui->sizePair->first()->setDecimals(NumberRange::decimals);
    _ui->sizePair->second()->setRange(0, NumberRange::maximum);
    _ui->sizePair->second()->setDecimals(NumberRange::decimals);

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
        _ui->sizePair->first()->setValue(rect.width());
        _ui->sizePair->second()->setValue(rect.height());
        if (rect.height() > 0) {
            _whRatio = rect.width() / rect.height();
        }
    });

    // Read: aspect-ratio lock state comes from the selected elements
    // (inkscape:preserve-aspect-ratio); mixed selections show as unlocked.
    binder.bindField(Props::Field::preserve_aspect_ratio, [this](const Props::SelectionState& s) {
        auto scoped = _update.block();
        _ui->sizePair->setLinked(s.element.preserve_aspect_ratio.is_single() &&
                                 s.element.preserve_aspect_ratio.value());
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
            applyTransform(x, _ui->yEdit->value(), _ui->sizePair->first()->value(), _ui->sizePair->second()->value());
        }));
    binder.track(connect(_ui->yEdit, &NumberEdit::valueChanged, this,
        [this, applyTransform](double y) {
            applyTransform(_ui->xEdit->value(), y, _ui->sizePair->first()->value(), _ui->sizePair->second()->value());
        }));
    binder.track(connect(_ui->sizePair->first(), &NumberEdit::valueChanged, this,
        [this, applyTransform](double w) {
            double h = _ui->sizePair->second()->value();
            if (_ui->sizePair->isLinked() && _whRatio > 0) {
                h = w / _whRatio;
                auto scoped = _update.block();
                _ui->sizePair->second()->setValue(h);
            }
            applyTransform(_ui->xEdit->value(), _ui->yEdit->value(), w, h);
        }));
    binder.track(connect(_ui->sizePair->second(), &NumberEdit::valueChanged, this,
        [this, applyTransform](double h) {
            double w = _ui->sizePair->first()->value();
            if (_ui->sizePair->isLinked() && _whRatio > 0) {
                w = h * _whRatio;
                auto scoped = _update.block();
                _ui->sizePair->first()->setValue(w);
            }
            applyTransform(_ui->xEdit->value(), _ui->yEdit->value(), w, h);
        }));

    // Write: toggle persists on the selected elements (inkscape:preserve-aspect-ratio).
    // Capturing the ratio on link makes the first resize preserve it.
    binder.track(connect(_ui->sizePair, &LinkedNumberPair::linkToggled, this,
        [this, editor](bool linked) {
            if (_update.pending()) return;
            if (linked) {
                auto h = _ui->sizePair->second()->value();
                if (h > 0) {
                    _whRatio = _ui->sizePair->first()->value() / h;
                }
            }
            editor->set(Props::preserve_aspect_ratio, linked);
        }));

    binder.track(connect(_ui->roundButton, &QPushButton::clicked, this,
        [this, applyTransform]() {
            {
                auto scoped = _update.block();
                _ui->xEdit->setValue(std::round(_ui->xEdit->value()));
                _ui->yEdit->setValue(std::round(_ui->yEdit->value()));
            }
            applyTransform(_ui->xEdit->value(), _ui->yEdit->value(),
                           _ui->sizePair->first()->value(), _ui->sizePair->second()->value());
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
