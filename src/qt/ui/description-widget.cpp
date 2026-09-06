// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * DescriptionWidget implementation.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "description-widget.h"

#include <QLineEdit>
#include <QPushButton>

#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "id-clash.h"
#include "object/sp-object.h"
#include "props/binder.h"
#include "props/editor.h"
#include "selection.h"
#include "ui_description-widget.h"
#include "util-string/context-string.h"

namespace Linea::UI {

DescriptionWidget::DescriptionWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::DescriptionWidget>()) {
    _ui->setupUi(this);

    // Validate ID on every change and enable/disable the Set button.
    QObject::connect(_ui->idEdit, &QLineEdit::textChanged, _ui->idEdit, [this]() {
        if (_update.pending()) return;
        validateId();
    });

    // Apply the new ID on Set button click.
    QObject::connect(_ui->setButton, &QPushButton::clicked, _ui->setButton, [this]() {
        if (_update.pending()) return;
        setId();
    });
}

DescriptionWidget::~DescriptionWidget() = default;

void DescriptionWidget::bind(Props::Binder& binder) {
    _editor = binder.editor();
    auto desktop = binder.editor()->desktop();
    if (!desktop) return;
    _document = desktop->getDocument();

    binder.bind(Props::title, _ui->titleEdit);
    binder.bind(Props::description, _ui->descriptionEdit);

    // ID row visible only for single-item selection.
    binder.visibleWhen(_ui->idLabel, Props::Cond::singleSelection);
    binder.visibleWhen(_ui->idEdit, Props::Cond::singleSelection);
    binder.visibleWhen(_ui->setButton, Props::Cond::singleSelection);

    // ID is read-only via the property system; the Set button handles the
    // write with validation. Track the selected item for the write path.
    binder.bindField(Props::Field::id, [this, desktop](const Props::SelectionState& s) {
        _item = desktop->getSelection()->singleItem();
        auto scoped = _update.block();
        if (s.element.id.is_single()) {
            _ui->idEdit->setText(QString::fromStdString(s.element.id.value()));
        } else {
            _ui->idEdit->clear();
        }
        validateId();
    });
}

void DescriptionWidget::validateId() {
    if (!_item || !_document) {
        _ui->setButton->setEnabled(false);
        return;
    }

    auto id = _ui->idEdit->text().toStdString();
    auto [valid, warning] = is_object_id_valid(id);
    if (valid) {
        auto current = _item->getId();
        if (!current) current = "";
        if (id != current && _document->getObjectById(id)) {
            valid = false;
            warning = _("This ID is already in use");
        }
    }

    _ui->idEdit->setProperty("warning", !valid);
    _ui->idEdit->style()->unpolish(_ui->idEdit);
    _ui->idEdit->style()->polish(_ui->idEdit);
    _ui->idEdit->setToolTip(QString::fromStdString(warning));
    _ui->setButton->setEnabled(valid && !id.empty());
}

void DescriptionWidget::setId() {
    if (!_item || !_document) return;

    auto id = _ui->idEdit->text().toStdString();
    auto [valid, warning] = is_object_id_valid(id);
    if (!valid) return;

    // Double-check uniqueness.
    auto current = _item->getId();
    if (!current) current = "";
    if (id != current && _document->getObjectById(id)) return;

    auto scoped = _update.block();
    _item->setAttribute("id", id);
    //todo: is this needed?
    _item->requestModified(SP_OBJECT_MODIFIED_FLAG);
    Inkscape::DocumentUndo::done(_document, RC_("Undo", "Set object ID"), "");
}

} // namespace Linea::UI
