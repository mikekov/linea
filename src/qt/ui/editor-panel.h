// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * EditorPanel — reusable multiline text editor widget with a search field
 * and an OK button. Designed to be embedded in a PopupMenu (which provides
 * the resizable frame via PopupMenu::setResizable).
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 */

#ifndef LINEA_UI_EDITOR_PANEL_H
#define LINEA_UI_EDITOR_PANEL_H

#include <QWidget>
#include <memory>

namespace Ui {
class EditorPanel;
}

namespace Linea::UI {

/**
 * Reusable multiline text editor panel.
 *
 * Layout: header row (title + search field), multiline editor body, footer
 * row (hint + OK button). The search field highlights all matches in the
 * editor and moves the caret to the first one.
 */
class EditorPanel : public QWidget {
    Q_OBJECT

public:
    explicit EditorPanel(QWidget* parent = nullptr);
    ~EditorPanel() override;

    void setText(const QString& text);
    QString text() const;

    void setTitle(const QString& title);

Q_SIGNALS:
    // Emitted when OK is pressed, or Shift+Return in the editor.
    // Read the edited text via text().
    void accepted();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateSearchHighlights(const QString& needle);

    std::unique_ptr<Ui::EditorPanel> ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_EDITOR_PANEL_H
