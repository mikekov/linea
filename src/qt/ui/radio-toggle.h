// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RadioToggle - Compound radio-style toggle widget.
 *
 * Contains a configurable number of exclusive push buttons.
 * Presents an int value and can show a mixed state where no button is checked.
 */

#ifndef LINEA_UI_RADIO_TOGGLE_H
#define LINEA_UI_RADIO_TOGGLE_H

#include <QPushButton>
#include <QStringList>
#include <QWidget>

#include <initializer_list>
#include <vector>

namespace Linea::UI {

/**
 * Compound widget with a configurable number of exclusive push buttons.
 *
 * One button is always checked for a single value; mixed state leaves all
 * buttons unchecked.
 */
class RadioToggle : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QStringList buttonIcons READ buttonIcons WRITE setButtonIcons)
    Q_PROPERTY(QStringList buttonToolTips READ buttonToolTips WRITE setButtonToolTips)
    Q_PROPERTY(QStringList buttonValues READ buttonValues WRITE setButtonValues)

public:
    explicit RadioToggle(QWidget* parent = nullptr);
    ~RadioToggle() override;

    int value() const;
    void setValue(int v);
    void setMixed(bool mixed);

    QStringList buttonIcons() const;
    void setButtonIcons(const QStringList& icons);
    QStringList buttonToolTips() const;
    void setButtonToolTips(const QStringList& tips);
    QStringList buttonValues() const;
    void setButtonValues(const QStringList& values);
    void setButtonValues(std::initializer_list<int> values);

    void setButtonIcon(int index, const QString& iconName);
    void setButtonToolTip(int index, const QString& text);
    void setButtonStyleClass(int index, const QString& styleClass);
    void setButtonEnabled(int index, bool enabled);

Q_SIGNALS:
    void valueChanged(int value);

private:
    void onClicked(int index);
    void setButtonCount(int count);
    void applyStyleClass(int index);
    void updateMixedStyle();
    void updateCheckState();

    std::vector<QPushButton*> _buttons;
    std::vector<int> _buttonValues;
    int _value = -1;
    bool _mixed = false;

    QStringList _buttonIcons;
    QStringList _buttonToolTips;
};

} // namespace Linea::UI

#endif // LINEA_UI_RADIO_TOGGLE_H
