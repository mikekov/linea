// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LINEA_UI_RADIO_BUTTON_GROUP_H
#define LINEA_UI_RADIO_BUTTON_GROUP_H

#include <QRadioButton>
#include <QStringList>
#include <QWidget>

#include <vector>

QT_BEGIN_NAMESPACE
class QBoxLayout;
QT_END_NAMESPACE

namespace Linea::UI {

class RadioButtonGroup : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool mixed READ mixed WRITE setMixed)
    Q_PROPERTY(QStringList buttonLabels READ buttonLabels WRITE setButtonLabels)
    Q_PROPERTY(QStringList buttonValues READ buttonValues WRITE setButtonValues)
    Q_PROPERTY(QStringList buttonIcons READ buttonIcons WRITE setButtonIcons)
    Q_PROPERTY(QStringList buttonToolTips READ buttonToolTips WRITE setButtonToolTips)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)

public:
    explicit RadioButtonGroup(QWidget* parent = nullptr);
    ~RadioButtonGroup() override;

    int value() const;
    void setValue(int v);
    bool mixed() const;
    void setMixed(bool mixed);

    QStringList buttonLabels() const;
    void setButtonLabels(const QStringList& labels);
    QStringList buttonValues() const;
    void setButtonValues(const QStringList& values);
    QStringList buttonIcons() const;
    void setButtonIcons(const QStringList& icons);
    QStringList buttonToolTips() const;
    void setButtonToolTips(const QStringList& tips);

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    void setButtonLabel(int index, const QString& label);
    void setButtonIcon(int index, const QString& iconName);
    void setButtonToolTip(int index, const QString& text);

Q_SIGNALS:
    void valueChanged(int value);

private:
    void onToggled(int index);
    void updateCheckState();
    void setButtonCount(int count);
    void applyButton(int index);
    void applyButtonValues();

    std::vector<QRadioButton*> _buttons;
    std::vector<int> _buttonValues;
    QBoxLayout* _layout = nullptr;
    int _value = -1;
    bool _mixed = false;
    QStringList _buttonLabels;
    QStringList _buttonValuesInput;
    QStringList _buttonIcons;
    QStringList _buttonToolTips;
    Qt::Orientation _orientation = Qt::Horizontal;
};

} // namespace Linea::UI

#endif // LINEA_UI_RADIO_BUTTON_GROUP_H
