// SPDX-License-Identifier: GPL-2.0-or-later

#include "radio-button-group.h"

#include <algorithm>

#include <QBoxLayout>
#include <QIcon>

namespace Linea::UI {

RadioButtonGroup::RadioButtonGroup(QWidget* parent)
    : QWidget(parent) {
    setObjectName("RadioButtonGroup");
    setProperty("class", "RadioButtonGroup");

    _layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(2);
}

RadioButtonGroup::~RadioButtonGroup() = default;

int RadioButtonGroup::value() const {
    return _value;
}

void RadioButtonGroup::setValue(int v) {
    if (v == _value) {
        return;
    }
    if (v != -1) {
        auto it = std::find(_buttonValues.begin(), _buttonValues.end(), v);
        if (it == _buttonValues.end()) {
            return;
        }
    }

    _value = v;
    _mixed = false;
    updateCheckState();
    Q_EMIT valueChanged(_value);
}

bool RadioButtonGroup::mixed() const {
    return _mixed;
}

void RadioButtonGroup::setMixed(bool mixed) {
    if (_mixed == mixed) {
        return;
    }
    _mixed = mixed;
    if (mixed) {
        _value = -1;
    }
    updateCheckState();
}

QStringList RadioButtonGroup::buttonLabels() const {
    return _buttonLabels;
}

void RadioButtonGroup::setButtonLabels(const QStringList& labels) {
    _buttonLabels = labels;
    if (!labels.isEmpty()) {
        setButtonCount(labels.size());
    }
}

QStringList RadioButtonGroup::buttonValues() const {
    return _buttonValuesInput;
}

void RadioButtonGroup::setButtonValues(const QStringList& values) {
    _buttonValuesInput = values;
    applyButtonValues();
    updateCheckState();
}

QStringList RadioButtonGroup::buttonIcons() const {
    return _buttonIcons;
}

void RadioButtonGroup::setButtonIcons(const QStringList& icons) {
    _buttonIcons = icons;
    if (_buttons.empty() && !icons.isEmpty()) {
        setButtonCount(icons.size());
    } else {
        for (int i = 0; i < static_cast<int>(_buttons.size()) && i < icons.size(); ++i) {
            if (!icons[i].isEmpty()) {
                setButtonIcon(i, icons[i]);
            }
        }
    }
}

QStringList RadioButtonGroup::buttonToolTips() const {
    return _buttonToolTips;
}

void RadioButtonGroup::setButtonToolTips(const QStringList& tips) {
    _buttonToolTips = tips;
    for (int i = 0; i < static_cast<int>(_buttons.size()) && i < tips.size(); ++i) {
        if (!tips[i].isEmpty()) {
            setButtonToolTip(i, tips[i]);
        }
    }
}

Qt::Orientation RadioButtonGroup::orientation() const {
    return _orientation;
}

void RadioButtonGroup::setOrientation(Qt::Orientation orientation) {
    if (_orientation == orientation) {
        return;
    }
    _orientation = orientation;
    _layout->setDirection(orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
}

void RadioButtonGroup::setButtonLabel(int index, const QString& label) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setText(label);
}

void RadioButtonGroup::setButtonIcon(int index, const QString& iconName) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setIcon(QIcon(":/icons/" + iconName));
}

void RadioButtonGroup::setButtonToolTip(int index, const QString& text) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setToolTip(text);
}

void RadioButtonGroup::onToggled(int index) {
    if (index < 0 || index >= static_cast<int>(_buttonValues.size())) {
        return;
    }
    int v = _buttonValues[index];
    if (_mixed || v != _value) {
        setValue(v);
    }
}

void RadioButtonGroup::updateCheckState() {
    for (int i = 0; i < static_cast<int>(_buttons.size()); ++i) {
        _buttons[i]->setChecked(!_mixed && _value != -1 && _buttonValues[i] == _value);
    }
}

void RadioButtonGroup::setButtonCount(int count) {
    if (count == static_cast<int>(_buttons.size())) {
        for (int i = 0; i < count; ++i) {
            applyButton(i);
        }
        return;
    }

    for (auto button : _buttons) {
        delete button;
    }
    _buttons.clear();
    _value = -1;
    _mixed = false;

    for (int i = 0; i < count; ++i) {
        auto button = new QRadioButton(this);
        button->setObjectName(QString("radioButton%1").arg(i));
        connect(button, &QRadioButton::toggled, this, [this, i](bool checked) {
            if (checked) {
                onToggled(i);
            }
        });
        _buttons.push_back(button);
        _layout->addWidget(button);
    }

    for (int i = 0; i < count; ++i) {
        applyButton(i);
    }
    applyButtonValues();
    updateCheckState();
}

void RadioButtonGroup::applyButtonValues() {
    _buttonValues.resize(_buttons.size());
    for (int i = 0; i < static_cast<int>(_buttons.size()); ++i) {
        if (i < _buttonValuesInput.size()) {
            _buttonValues[i] = _buttonValuesInput[i].toInt();
        } else {
            _buttonValues[i] = i;
        }
    }
}

void RadioButtonGroup::applyButton(int index) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    if (index < _buttonLabels.size() && !_buttonLabels[index].isEmpty()) {
        _buttons[index]->setText(_buttonLabels[index]);
    }
    if (index < _buttonIcons.size() && !_buttonIcons[index].isEmpty()) {
        _buttons[index]->setIcon(QIcon(":/icons/" + _buttonIcons[index]));
    }
    if (index < _buttonToolTips.size() && !_buttonToolTips[index].isEmpty()) {
        _buttons[index]->setToolTip(_buttonToolTips[index]);
    }
}

} // namespace Linea::UI
