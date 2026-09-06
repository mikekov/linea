// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PanelSwitch widget implementation.
 */

#include "panel-switch.h"

#include <QHBoxLayout>
#include <QPushButton>

namespace Linea::UI {

PanelSwitch::PanelSwitch(QWidget* parent)
    : QWidget(parent) {
    setProperty("class", "PanelSwitch");
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch();
}

PanelSwitch::~PanelSwitch() = default;

int PanelSwitch::addButton(const QString& iconName, const QString& label, const QString& tooltip) {
    auto button = new QPushButton(this);
    button->setCheckable(true);
    button->setIcon(QIcon(":/icons/" + iconName));
    if (!label.isEmpty()) {
        button->setText(label);
    }
    else {
        // button->setMinimumWidth(50);
    }
    if (!tooltip.isEmpty()) {
        button->setToolTip(tooltip);
    }

    int index = static_cast<int>(_buttons.size());
    connect(button, &QPushButton::clicked, this, [this, index] { onButtonClicked(index); });

    auto layout = qobject_cast<QHBoxLayout*>(this->layout());
    if (layout) {
        // Insert before the trailing stretch so buttons pack left.
        layout->insertWidget(layout->count() - 1, button);
    }

    _buttons.push_back({button, iconName});

    // Select the first button by default.
    if (_current == -1) {
        setCurrentIndex(0);
    }

    return index;
}

int PanelSwitch::currentIndex() const {
    return _current;
}

void PanelSwitch::setCurrentIndex(int index) {
    if (index < 0 || index >= static_cast<int>(_buttons.size())) return;
    if (index == _current) return;

    _current = index;
    for (int i = 0; i < static_cast<int>(_buttons.size()); ++i) {
        _buttons[i].button->setChecked(i == _current);
    }
    Q_EMIT currentChanged(_current);
}

void PanelSwitch::onButtonClicked(int index) {
    if (index == _current) {
        // Clicking the active button must not unselect it; Qt toggled it
        // off, so re-assert the checked state.
        _buttons[index].button->setChecked(true);
        return;
    }
    setCurrentIndex(index);
}

} // namespace Linea::UI
