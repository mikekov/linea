// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Floating, resizable dialog hosting the ScriptEditorPanel.
 */

#include "scripting-dialog.h"

#include <QVBoxLayout>

#include "qt/ui/script-editor-panel.h"
#include "qt/ui/widget-utils.h"

class LineaApplication;

namespace Linea::UI {

ScriptingDialog::ScriptingDialog(LineaApplication& app, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Scripting"));
    setObjectName(QStringLiteral("ScriptingDialog"));

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new ScriptEditorPanel(app, this));

    persistGeometry(this, "/dialogs/scripting", {760, 520});
}

ScriptingDialog::~ScriptingDialog() = default;

} // namespace Linea::UI
