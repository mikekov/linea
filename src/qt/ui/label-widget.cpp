// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LabelWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "label-widget.h"

#include <QAction>
#include <QLineEdit>
#include <QToolButton>

#include "desktop.h"
#include "description-widget.h"
#include "document-undo.h"
#include "object/sp-object.h"
#include "popup-menu.h"
#include "props/binder.h"
#include "props/editor.h"
#include "props/selection-state.h"
#include "selection.h"
#include "transform-panel.h"
#include "ui/util.h"
#include "widget-utils.h"
#include "ui_label-widget.h"
#include "util-string/context-string.h"

namespace Linea::UI {

LabelWidget::LabelWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::LabelWidget>()) {
    _ui->setupUi(this);
    fixColumnToolButtons(this);

    // Build popup with DescriptionWidget as content
    _popup = new PopupMenu(this);
    _descriptionWidget = new DescriptionWidget();
    _descriptionWidget->setFixedSize(260, 240);
    _popup->setContent(_descriptionWidget);

    // Embedded "edit description" icon inside the line edit
    auto action = _ui->lineEdit->addAction(QIcon(":/icons/edit"), QLineEdit::TrailingPosition);
    action->setToolTip(tr("Edit description"));
    connect(action, &QAction::triggered, this, [this]() {
        _popup->showBelowWidget(_ui->lineEdit);
    });
    UI::highlightEmbeddedIconsOnHover(_ui->lineEdit);

    connect(_ui->transform, &QToolButton::clicked, this, &LabelWidget::onTransformClicked);

    // Prevent horizontal stretching
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

LabelWidget::~LabelWidget() = default;

void LabelWidget::bind(Props::Binder& binder) {
    _desktop = binder.editor()->desktop();
    if (!_desktop) return;

    // Read: refresh the line edit on selection change or ID change (the
    // synthetic name is derived from the ID, so an ID change must refresh).
    // The display value is selection-level, not a per-item merge:
    //   - single item: show its label, or a synthetic name if unlabeled
    //   - multiple items: show "N items" and disable editing
    //   - empty: cleared and disabled
    binder.bindField(Props::Field::counts, [this](const Props::SelectionState&) {
        refreshLabel();
    });

    binder.bindField(Props::Field::id, [this](const Props::SelectionState&) {
        refreshLabel();
    });

    // Write: only meaningful for single-item selection. Editing the label
    // sets inkscape:label on the object and records one undo step.
    auto tag = binder.editor()->tag();
    binder.track(QObject::connect(_ui->lineEdit, &QLineEdit::editingFinished, _ui->lineEdit,
        [this, tag]() {
            if (_update.pending()) return;
            auto sel = _desktop->getSelection();
            if (!sel) return;
            auto item = sel->single();
            if (!item || !item->document) return;

            auto new_label = _ui->lineEdit->text().toStdString();
            auto current = item->label();
            if (new_label == (current ? current : "")) return;

            item->setLabel(new_label.empty() ? nullptr : new_label.c_str());
            Inkscape::DocumentUndo::done(item->document, RC_("Undo", "Set label"), "", tag);
        }));

    // Bind the description popup's title and description fields.
    _descriptionWidget->bind(binder);

    // Transform applies to items only — hide the button when the selection
    // holds objects but no items.
    binder.visibleWhen(_ui->transform, Props::Cond::hasSelection);

    // Visible only when there's a selection.
    binder.visibleWhen(this, Props::Cond::hasSelection || Props::Cond::hasPageSelection);
}

void LabelWidget::refreshLabel() {
    auto scoped = _update.block();
    auto sel = _desktop->getSelection();
    if (!sel || sel->isEmpty()) {
        _ui->lineEdit->clear();
        _ui->lineEdit->setEnabled(false);
        return;
    }
    auto item = sel->single();
    if (!item) {
        _ui->lineEdit->setText(tr("%n items", nullptr, static_cast<int>(sel->size())));
        _ui->lineEdit->setEnabled(false);
        return;
    }
    // user-defined label takes priority; fall back to synthetic name
    if (auto label = item->label()) {
        _ui->lineEdit->setText(QString::fromUtf8(label));
    } else {
        _ui->lineEdit->setText(get_synthetic_object_name(item));
    }
    _ui->lineEdit->setEnabled(true);
}

void LabelWidget::onTransformClicked() {
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
