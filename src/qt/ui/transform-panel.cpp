// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * TransformPanel — move/scale/rotate/skew and matrix transform panel
 */
/*
 * Authors:
 *   Mike Kowalski
 *
 * Copyright (C) 2025-2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "transform-panel.h"
#include "ui_transform-panel.h"

#include <QCheckBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QPushButton>
#include <algorithm>

#include <2geom/affine.h>

#include "desktop.h"
#include "document-undo.h"
#include "preferences.h"
#include "selection.h"
#include "number-edit.h"
#include "tab-strip.h"
#include "util/transform-objects.h"
#include "util-string/context-string.h"

using Inkscape::DocumentUndo;
using Inkscape::Preferences;

namespace Linea::UI {

static constexpr const char* kPrefPage        = "/panels/transform/current-page";
static constexpr const char* kPrefReplaceMatrix = "/panels/transform/replace-matrix";
static constexpr const char* kPrefLinkedScale   = "/panels/transform/linked-scale";

TransformPanel::TransformPanel(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::TransformPanel>())
{
    _ui->setupUi(this);

    // setMinimumWidth(std::max(_ui->transformPage->sizeHint().width(),
                            //  _ui->matrixPage->sizeHint().width()));

    // restore persisted state before setting up UI that reads it
    _linkedScale = Preferences::get()->getBool(kPrefLinkedScale, false);
    _curPage     = Preferences::get()->getInt(kPrefPage, PageTransforms);
    _ui->currentMatrix->setChecked(Preferences::get()->getBool(kPrefReplaceMatrix, false));

    setupTabStrip();
    setupIcons();
    setupConnections();

    resetToDefaults();
    showPage(_curPage);
}

TransformPanel::~TransformPanel() = default;

void TransformPanel::setupTabStrip() {
    auto tabs = _ui->tabStrip;
    tabs->setRearrangingTabs(TabStrip::Rearrange::Never);
    tabs->setShowLabels(TabStrip::ShowLabels::Always);
    tabs->setShowCloseButton(false);

    _tabTransform = tabs->addTab(tr("Transform"), "dialog-transform");
    _tabMatrix    = tabs->addTab(tr("Matrix"),    "matrix");

    connect(tabs, &TabStrip::tabSelectRequested, this, [this](QWidget* tab) {
        _ui->tabStrip->selectTab(*tab);
        auto page = (tab == _tabTransform) ? PageTransforms : PageMatrix;
        showPage(page);
        Preferences::get()->setInt(kPrefPage, page);
    });
}

void TransformPanel::setupIcons() {
    // _ui->resetBtn->setIcon(QIcon(":/icons/reset-settings"));
    _ui->linkScaleBtn->setIcon(QIcon(_linkedScale ? ":/icons/entries-linked" : ":/icons/entries-unlinked"));
}

void TransformPanel::setupConnections() {
    // Apply / Duplicate buttons
    connect(_ui->applyBtn,     &QPushButton::clicked, this, [this]{ onApply(false); });
    connect(_ui->duplicateBtn, &QPushButton::clicked, this, [this]{ onApply(true);  });

    // Reset button
    connect(_ui->resetBtn, &QToolButton::clicked, this, [this]{ resetToDefaults(); });

    // Scale link
    connect(_ui->scaleX, &NumberEdit::valueChanged, this, [this](double v) {
        if (_linkedScale) _ui->scaleY->setValue(v);
    });
    connect(_ui->scaleY, &NumberEdit::valueChanged, this, [this](double v) {
        if (_linkedScale) _ui->scaleX->setValue(v);
    });
    connect(_ui->linkScaleBtn, &QToolButton::clicked, this, [this] {
        _linkedScale = !_linkedScale;
        Preferences::get()->setBool(kPrefLinkedScale, _linkedScale);
        if (_linkedScale) _ui->scaleY->setValue(_ui->scaleX->value());
        _ui->linkScaleBtn->setIcon(QIcon(_linkedScale ? ":/icons/entries-linked" : ":/icons/entries-unlinked"));
    });

    // current-matrix checkbox
    connect(_ui->currentMatrix, &QCheckBox::toggled, this, [this](bool checked) {
        if (!_desktop) return;
        Preferences::get()->setBool(kPrefReplaceMatrix, checked);
        if (checked) {
            updateUi(*_desktop->getSelection());
        } else {
            clearMatrix();
        }
    });
}

void TransformPanel::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void TransformPanel::updateUi(Inkscape::Selection& selection) {
    const bool enable = !selection.isEmpty();

    if (enable && _ui->currentMatrix->isChecked()) {
        // fill matrix from the first selected item
        const auto& m = selection.items().front()->transform;
        _ui->matrixA->setValue(m[0]);
        _ui->matrixB->setValue(m[1]);
        _ui->matrixC->setValue(m[2]);
        _ui->matrixD->setValue(m[3]);
        _ui->matrixE->setValue(m[4]);
        _ui->matrixF->setValue(m[5]);
    }

    // switch replace-matrix label for single vs. multi selection
    _ui->replaceMatrixStack->setCurrentIndex(selection.size() > 1 ? 1 : 0);

    // "transform each separately" only makes sense with multiple objects
    _ui->objSeparately->setVisible(selection.size() > 1);

    _ui->applyBtn->setEnabled(enable);
    _ui->duplicateBtn->setEnabled(enable);
}

void TransformPanel::setPage(int page) {
    showPage(page);
}

// ── private ──────────────────────────────────────────────────────────────────

void TransformPanel::showPage(int page) {
    _curPage = page;
    _ui->transformPage->setVisible(page == PageTransforms);
    _ui->transformPage->setEnabled(page == PageTransforms);
    _ui->matrixPage->setVisible(page == PageMatrix);
    _ui->matrixPage->setEnabled(page == PageMatrix);

    auto* tab = (page == PageTransforms) ? _tabTransform : _tabMatrix;
    if (tab) _ui->tabStrip->selectTab(*tab);
}

void TransformPanel::resetToDefaults() {
    _ui->currentMatrix->setChecked(false);
    clearMatrix();

    _ui->moveX->setValue(0);
    _ui->moveY->setValue(0);
    _ui->relativeMove->setChecked(true);
    _ui->rotate->setValue(0);
    _ui->scaleX->setValue(100);
    _ui->scaleY->setValue(100);
    _ui->skewX->setValue(0);
    _ui->skewY->setValue(0);
}

void TransformPanel::clearMatrix() {
    _ui->matrixA->setValue(1);
    _ui->matrixB->setValue(0);
    _ui->matrixC->setValue(0);
    _ui->matrixD->setValue(1);
    _ui->matrixE->setValue(0);
    _ui->matrixF->setValue(0);
}

void TransformPanel::onApply(bool duplicate) {
    if (!_desktop) return;

    auto selection = _desktop->getSelection();
    if (!selection || selection->isEmpty()) return;

    if (_curPage == PageMatrix) {
        // read matrix values before duplicating (duplicate can change selection)
        const double a = _ui->matrixA->value();
        const double b = _ui->matrixB->value();
        const double c = _ui->matrixC->value();
        const double d = _ui->matrixD->value();
        const double e = _ui->matrixE->value();
        const double f = _ui->matrixF->value();
        Geom::Affine matrix(a, b, c, d, e, f);

        if (duplicate) selection->duplicate();

        const bool replace = _ui->currentMatrix->isChecked();
        transform_apply_matrix(selection, matrix, replace);

        DocumentUndo::done(_desktop->getDocument(),
            duplicate ? RC_("Undo", "Duplicate selection and edit transformation matrix")
                      : RC_("Undo", "Edit transformation matrix"),
            "dialog-transform");
        return;
    }

    // PageTransforms
    if (duplicate) selection->duplicate();

    const bool applySeparately = _ui->objSeparately->isChecked();
    bool changed = false;

    const bool relative = _ui->relativeMove->isChecked();
    const double mx = _ui->moveX->value();
    const double my = _ui->moveY->value();
    if (!relative || mx != 0 || my != 0) {
        transform_move(selection, mx, my, relative, applySeparately, _desktop->yaxisdir());
        changed = true;
    }

    const double angle = _ui->rotate->value();
    if (angle != 0) {
        transform_rotate(selection, angle, applySeparately);
        changed = true;
    }

    const double sx = _ui->scaleX->value();
    const double sy = _ui->scaleY->value();
    if (sx != 100 || sy != 100) {
        const bool transformStroke = Preferences::get()->getBool("/options/transform/stroke", true);
        const bool preserve        = Preferences::get()->getBool("/options/preservetransform/value", false);
        transform_scale(selection, sx, sy, true, applySeparately, transformStroke, preserve);
        changed = true;
    }

    const double hx = _ui->skewX->value();
    const double hy = _ui->skewY->value();
    if (hx != 0 || hy != 0) {
        transform_skew(selection, hx, hy, SkewUnits::Absolute, applySeparately, _desktop->yaxisdir());
        changed = true;
    }

    if (changed) {
        DocumentUndo::done(_desktop->getDocument(),
            duplicate ? RC_("Undo", "Duplicate and transform selection")
                      : RC_("Undo", "Transform selection"),
            "dialog-transform");
    }
}

} // namespace Linea::UI
