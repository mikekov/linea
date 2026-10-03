// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * EditorPanel — reusable multiline text editor widget.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 */

#include "editor-panel.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

#include "ui_editor-panel.h"

namespace Linea::UI {

EditorPanel::EditorPanel(QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::EditorPanel>()) {
    ui->setupUi(this);

    connect(ui->searchEntry, &QLineEdit::textChanged, this, &EditorPanel::updateSearchHighlights);
    connect(ui->okButton, &QPushButton::clicked, this, &EditorPanel::accepted);
    ui->textEdit->installEventFilter(this);
}

EditorPanel::~EditorPanel() = default;

void EditorPanel::setText(const QString& text) {
    ui->textEdit->setPlainText(text);
    updateSearchHighlights(ui->searchEntry->text());
}

QString EditorPanel::text() const {
    return ui->textEdit->toPlainText();
}

void EditorPanel::setTitle(const QString& title) {
    ui->headerLabel->setText(title);
}

bool EditorPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->textEdit && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            (keyEvent->modifiers() & Qt::ShiftModifier)) {
            Q_EMIT accepted();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EditorPanel::updateSearchHighlights(const QString& needle) {
    QList<QTextEdit::ExtraSelection> selections;

    if (!needle.isEmpty()) {
        auto doc = ui->textEdit->document();
        QColor highlight = palette().color(QPalette::Highlight);
        highlight.setAlphaF(0.5);

        QTextCursor cursor(doc);
        while (!(cursor = doc->find(needle, cursor)).isNull()) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(highlight);
            selections.append(selection);
        }

        // move the caret to the first match so it scrolls into view
        QTextCursor first = doc->find(needle, 0);
        if (!first.isNull()) {
            ui->textEdit->setTextCursor(first);
        }
    }

    ui->textEdit->setExtraSelections(selections);
}

} // namespace Linea::UI
