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

#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <QPushButton>

#include "desktop.h"
#include "description-widget.h"
#include "document-undo.h"
#include "object/sp-object.h"
#include "popup-menu.h"
#include "props/binder.h"
#include "props/editor.h"
#include "props/selection-state.h"
#include "selection.h"
#include "ui/util.h"
#include "util-string/context-string.h"

namespace Linea::UI {

LabelWidget::LabelWidget(QWidget* parent)
    : QWidget(parent) {
    _layout = new QGridLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(4);
    _layout->setColumnStretch(0, 1);

    // Create label
    _label = new QLabel(tr("Selection"), this);
    _label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _label->setProperty("class", "panel-label");
    _label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _layout->addWidget(_label, 0, 0);

    // Create QLineEdit for editable label
    _lineEdit = new QLineEdit(this);
    _lineEdit->setPlaceholderText(tr("Enter label..."));
    _layout->addWidget(_lineEdit, 1, 0);

    // Create button to open description editor
    _button = new QPushButton(this);
    _button->setIcon(QIcon(":/icons/text-input"));
    _button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _layout->addWidget(_button, 1, 1);

    // Build popup with DescriptionWidget as content
    _popup = new PopupMenu(this);
    _descriptionWidget = new DescriptionWidget();
    _descriptionWidget->setFixedSize(260, 240);
    _popup->setContent(_descriptionWidget);

    connect(_button, &QPushButton::clicked, this, [this]() {
        _popup->showBelowWidget(_button);
    });

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
    binder.track(QObject::connect(_lineEdit, &QLineEdit::editingFinished, _lineEdit,
        [this, tag]() {
            if (_update.pending()) return;
            auto sel = _desktop->getSelection();
            if (!sel) return;
            auto item = sel->single();
            if (!item || !item->document) return;

            auto new_label = _lineEdit->text().toStdString();
            auto current = item->label();
            if (new_label == (current ? current : "")) return;

            item->setLabel(new_label.empty() ? nullptr : new_label.c_str());
            Inkscape::DocumentUndo::done(item->document, RC_("Undo", "Set label"), "", tag);
        }));

    // Bind the description popup's title and description fields.
    _descriptionWidget->bind(binder);

    // Visible only when there's a selection.
    binder.visibleWhen(this, Props::Cond::hasSelection || Props::Cond::hasPageSelection);
}

void LabelWidget::refreshLabel() {
    auto scoped = _update.block();
    auto sel = _desktop->getSelection();
    if (!sel || sel->isEmpty()) {
        _lineEdit->clear();
        _lineEdit->setEnabled(false);
        return;
    }
    auto item = sel->single();
    if (!item) {
        _lineEdit->setText(tr("%n items", nullptr, static_cast<int>(sel->size())));
        _lineEdit->setEnabled(false);
        return;
    }
    // user-defined label takes priority; fall back to synthetic name
    if (auto label = item->label()) {
        _lineEdit->setText(QString::fromUtf8(label));
    } else {
        _lineEdit->setText(get_synthetic_object_name(item));
    }
    _lineEdit->setEnabled(true);
}

} // namespace Linea::UI
