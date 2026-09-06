// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PanelSwitch — horizontal strip of exclusive push buttons for switching panels.
 *
 * Each button has an icon and an optional label. Exactly one button is active
 * at a time. The active/inactive backgrounds are styled via styles.qss using
 * the `PanelSwitch` class selector and the buttons' `:checked` pseudo-state.
 */

#ifndef LINEA_UI_PANEL_SWITCH_H
#define LINEA_UI_PANEL_SWITCH_H

#include <QIcon>
#include <QString>
#include <QWidget>

#include <vector>

class QPushButton;

namespace Linea::UI {

class PanelSwitch : public QWidget {
    Q_OBJECT

public:
    explicit PanelSwitch(QWidget* parent = nullptr);
    ~PanelSwitch() override;

    /// Add a button with an icon and optional label. Returns its index.
    int addButton(const QString& iconName, const QString& label = {}, const QString& tooltip = {});

    /// Currently active button index, or -1 if none.
    int currentIndex() const;

    /// Select the active button by index.
    void setCurrentIndex(int index);

Q_SIGNALS:
    /// Emitted when the active button changes.
    void currentChanged(int index);

private:
    void onButtonClicked(int index);

    struct Button {
        QPushButton* button;
        QString iconName;
    };
    std::vector<Button> _buttons;
    int _current = -1;
};

} // namespace Linea::UI

#endif // LINEA_UI_PANEL_SWITCH_H
