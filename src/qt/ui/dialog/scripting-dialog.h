// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Floating, resizable dialog hosting the ScriptEditorPanel.
 *
 * Size and position are persisted in preferences under /dialogs/scripting/
 * and restored on the next session; a saved position on a disconnected
 * display falls back to a visible screen (see persistGeometry).
 */

#ifndef LINEA_UI_DIALOG_SCRIPTING_DIALOG_H
#define LINEA_UI_DIALOG_SCRIPTING_DIALOG_H

#include <QDialog>

class LineaApplication;

namespace Linea::UI {

class ScriptingDialog : public QDialog {
    Q_OBJECT

public:
    explicit ScriptingDialog(LineaApplication& app, QWidget* parent = nullptr);
    ~ScriptingDialog() override;
};

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_SCRIPTING_DIALOG_H
