// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RadioToggle widget implementation.
 */

#include "radio-toggle.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QStyle>

namespace Linea::UI {

RadioToggle::RadioToggle(QWidget* parent)
    : QWidget(parent)
    , _value(-1)
{
    setObjectName("RadioToggle");
    setProperty("class", "RadioToggle");
    setAttribute(Qt::WA_StyledBackground, true);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
}

RadioToggle::~RadioToggle() = default;

int RadioToggle::value() const {
    return _value;
}

void RadioToggle::setValue(int v) {
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
    if (_mixed) {
        _mixed = false;
        updateMixedStyle();
    }
    updateCheckState();
    Q_EMIT valueChanged(_value);
}

void RadioToggle::setMixed(bool mixed) {
    if (_mixed == mixed) {
        return;
    }
    _mixed = mixed;
    if (mixed) {
        _value = -1;
    }
    updateCheckState();
    updateMixedStyle();
}

void RadioToggle::updateMixedStyle() {
    for (auto button : _buttons) {
        button->setProperty("mixed", _mixed);
        if (auto s = button->style(); s) {
            s->unpolish(button);
            s->polish(button);
        }
        button->update();
    }
}

QStringList RadioToggle::buttonIcons() const {
    return _buttonIcons;
}

void RadioToggle::setButtonIcons(const QStringList& icons) {
    _buttonIcons = icons;
    setButtonCount(icons.size());
    for (int i = 0; i < static_cast<int>(_buttons.size()); ++i) {
        if (i < icons.size() && !icons[i].isEmpty()) {
            setButtonIcon(i, icons[i]);
        }
    }
}

QStringList RadioToggle::buttonToolTips() const {
    return _buttonToolTips;
}

void RadioToggle::setButtonToolTips(const QStringList& tips) {
    _buttonToolTips = tips;
    for (int i = 0; i < static_cast<int>(_buttons.size()); ++i) {
        if (i < tips.size() && !tips[i].isEmpty()) {
            setButtonToolTip(i, tips[i]);
        }
    }
}

QStringList RadioToggle::buttonValues() const {
    assert(false); // we need write-only property
    return QStringList();
}

void RadioToggle::setButtonValues(const QStringList& values) {
    assert(_buttonValues.size() == values.size());

    for (size_t i = 0; i < values.size(); ++i) {
        if (i >= _buttonValues.size()) {
            break;
        }
        _buttonValues[i] = values[i].toInt();
    }
    updateCheckState();
}

void RadioToggle::setButtonValues(std::initializer_list<int> values) {
    assert(_buttonValues.size() == values.size());

    size_t i = 0;
    for (int v : values) {
        if (i >= _buttonValues.size()) {
            break;
        }
        _buttonValues[i++] = v;
    }
    updateCheckState();
}

void RadioToggle::setButtonCount(int count) {
    assert(count >= 0);
    if (count == static_cast<int>(_buttons.size())) {
        return;
    }

    for (auto button : _buttons) {
        delete button;
    }
    _buttons.clear();
    _value = -1;
    _mixed = false;

    auto layout = qobject_cast<QHBoxLayout*>(this->layout());
    if (!layout) {
        return;
    }

    _buttonValues.clear();
    _buttonValues.reserve(count);

    for (int i = 0; i < count; ++i) {
        auto button = new QPushButton(this);
        button->setObjectName(QString("radioButton%1").arg(i));
        button->setCheckable(true);
        connect(button, &QPushButton::clicked, this, [this, i] { onClicked(i); });
        _buttons.push_back(button);
        _buttonValues.push_back(i);
        layout->addWidget(button);
    }

    for (int i = 0; i < count; ++i) {
        applyStyleClass(i);
    }

    setButtonToolTips(_buttonToolTips);
    updateCheckState();
    updateMixedStyle();
}

void RadioToggle::setButtonIcon(int index, const QString& iconName) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setIcon(QIcon(":/icons/" + iconName));
}

void RadioToggle::setButtonToolTip(int index, const QString& text) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setToolTip(text);
}

void RadioToggle::setButtonStyleClass(int index, const QString& styleClass) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setProperty("class", styleClass);
}

void RadioToggle::setButtonEnabled(int index, bool enabled) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) {
        return;
    }
    _buttons[index]->setEnabled(enabled);
}

void RadioToggle::applyStyleClass(int index) {
    const int n = static_cast<int>(_buttons.size());
    if (n == 0 || index < 0 || index >= n) {
        return;
    }

    QString cls = "narrow-button ";
    if (index == 0) {
        cls += "segment-first";
    } else if (index == n - 1) {
        cls += "segment-last";
    } else {
        cls += "segment-middle";
    }
    _buttons[index]->setProperty("class", cls);
}

void RadioToggle::onClicked(int index) {
    if (index < 0 || index >= static_cast<int>(_buttonValues.size())) {
        return;
    }
    int v = _buttonValues[index];
    if (_mixed || v != _value) {
        setValue(v);
    }
}

void RadioToggle::updateCheckState() {
    for (int i = 0; i < static_cast<int>(_buttons.size()); ++i) {
        _buttons[i]->setChecked(!_mixed && _value != -1 && _buttonValues[i] == _value);
    }
}

} // namespace Linea::UI
