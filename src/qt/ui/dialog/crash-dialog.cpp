// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * CrashDialog — Qt crash report dialog implementation.
 */

#include "crash-dialog.h"

#include <QFontDatabase>

#include "ui_crash-dialog.h"

namespace Linea::UI {

CrashDialog::CrashDialog(const QString& autosaves, const QString& stacktrace, QWidget* parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::CrashDialog>()) {
    _ui->setupUi(this);
    _ui->autosavesLabel->setVisible(!autosaves.isEmpty());
    _ui->autosavesLabel->setText(autosaves);
    _ui->stacktraceEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    _ui->stacktraceEdit->setPlainText(stacktrace);
    _ui->okButton->setFocus();
}

CrashDialog::~CrashDialog() = default;

} // namespace Linea::UI
