// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Qt settings dialog (skeleton — grid layout defined in settings-dialog.ui).
 */

#ifndef LINEA_UI_DIALOG_SETTINGS_DIALOG_H
#define LINEA_UI_DIALOG_SETTINGS_DIALOG_H

#include <QDialog>
#include <memory>

namespace Ui {
class SettingsDialog;
}

namespace Linea::UI {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

private:
    std::unique_ptr<Ui::SettingsDialog> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_SETTINGS_DIALOG_H
