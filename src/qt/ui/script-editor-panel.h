// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LINEA_UI_SCRIPT_EDITOR_PANEL_H
#define LINEA_UI_SCRIPT_EDITOR_PANEL_H

#include <QWidget>
#include <memory>

class QLineEdit;
class QPlainTextEdit;

#include "script/script-error.h"

class LineaApplication;

namespace Ui {
class ScriptEditorPanel;
}

namespace Linea::UI {
namespace Syntax {
class TextEditView;
}

class ScriptEditorPanel : public QWidget {
public:
    explicit ScriptEditorPanel(LineaApplication& app, QWidget* parent = nullptr);
    ~ScriptEditorPanel() override;

    QString scriptName() const;
    QString scriptText() const;

private:
    void addScript();
    void deleteScript();
    void runCurrentScript();
    void loadSelectedScript(const QString& name);
    void showError(const Script::Error& error);

    LineaApplication& _app;
    std::unique_ptr<Ui::ScriptEditorPanel> _ui;
    std::unique_ptr<Syntax::TextEditView> _editor;
};

} // namespace Linea::UI

#endif // LINEA_UI_SCRIPT_EDITOR_PANEL_H
