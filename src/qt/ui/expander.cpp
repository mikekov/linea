// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Expander — collapsible section header button.
 */

#include "expander.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QToolButton>

namespace Linea::UI {

Expander::Expander(QWidget* parent)
    : QWidget(parent) {
    _button = new QToolButton(this);
    _button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _button->setIconSize(QSize(10, 16));
    setFocusProxy(_button);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_button);

    connect(_button, &QToolButton::clicked, this, [this] {
        setState(_state == Collapsed ? Expanded : Collapsed);
        Q_EMIT clicked();
    });

    updateIcon();
}

Expander::Expander(const QString& label, QWidget* parent)
    : Expander(parent) {
    setLabel(label);
}

QString Expander::label() const {
    return _button->text();
}

void Expander::setLabel(const QString& label) {
    _button->setText(label);
}

void Expander::setState(State state) {
    if (_state == state) {
        return;
    }
    _state = state;
    updateIcon();
    Q_EMIT stateChanged(_state);
}

void Expander::setExpanded(bool expanded) {
    setState(expanded ? Expanded : Collapsed);
}

void Expander::changeEvent(QEvent* event) {
    if (event->type() == QEvent::EnabledChange) {
        updateIcon();
    }
    QWidget::changeEvent(event);
}

void Expander::updateIcon() {
    _button->setIcon(QIcon(!isEnabled() ? ":/icons/undetermined"
                           : _state == Expanded ? ":/icons/expanded" : ":/icons/collapsed"));
}

} // namespace Linea::UI
