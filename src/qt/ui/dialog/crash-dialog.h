// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * CrashDialog — Qt crash report dialog.
 */

#ifndef LINEA_UI_DIALOG_CRASH_DIALOG_H
#define LINEA_UI_DIALOG_CRASH_DIALOG_H

#include <QDialog>
#include <memory>

namespace Ui {
class CrashDialog;
}

namespace Linea::UI {

class CrashDialog : public QDialog {
    Q_OBJECT

public:
    CrashDialog(const QString& autosaves, const QString& stacktrace, QWidget* parent = nullptr);
    ~CrashDialog() override;

private:
    std::unique_ptr<Ui::CrashDialog> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_CRASH_DIALOG_H
