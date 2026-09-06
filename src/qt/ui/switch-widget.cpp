// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SwitchWidget — a toggle switch control with animated knob.
 */

#include "switch-widget.h"

#include <QPainter>
#include <QPropertyAnimation>

namespace Linea::UI {

SwitchWidget::SwitchWidget(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    _animation = new QPropertyAnimation(this, "position", this);
    _animation->setDuration(120);

    connect(this, &QAbstractButton::toggled, this, &SwitchWidget::startAnimation);
}

QSize SwitchWidget::sizeHint() const {
    // Allow QSS min-width/min-height to override defaults
    const QSize min = minimumSize();
    return QSize(
        min.width()  > 0 ? min.width()  : 44,
        min.height() > 0 ? min.height() : 24
    );
}

void SwitchWidget::setPosition(qreal pos) {
    _position = pos;
    update();
}

void SwitchWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& pal = palette();
    const QRectF r(0, 0, width(), height());
    const qreal radius = r.height() / 2.0;

    // Track color: interpolate between button (off) and accent (on)
    QColor trackOff = pal.color(QPalette::Button);
    QColor trackOn = pal.color(QPalette::Accent);
    QColor borderColor = pal.color(QPalette::Mid);

    int red   = trackOff.red()   + _position * (trackOn.red()   - trackOff.red());
    int green = trackOff.green() + _position * (trackOn.green() - trackOff.green());
    int blue  = trackOff.blue()  + _position * (trackOn.blue()  - trackOff.blue());
    QColor trackColor(red, green, blue);

    // Draw track
    QPen border(Qt::PenStyle::SolidLine);
    border.setColor(borderColor);

    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(r, radius, radius);

    p.setPen(border);
    auto pw = p.pen().widthF();
    QRectF inset = r.adjusted(pw/2, pw/2, -pw/2, -pw/2);
    p.drawRoundedRect(inset, radius, radius);

    // Knob
    const qreal margin = 2.0;
    const qreal knobDiameter = r.height() - 2 * margin;
    const qreal xMin = margin;
    const qreal xMax = r.width() - margin - knobDiameter;
    const qreal knobX = xMin + _position * (xMax - xMin);

    // p.setPen(Qt::NoPen);
    p.setBrush(pal.color(QPalette::Base));
    p.drawEllipse(QRectF(knobX, margin, knobDiameter, knobDiameter));
}

void SwitchWidget::checkStateSet() {
    QAbstractButton::checkStateSet();
    // Snap position without animation when state is set programmatically
    _position = isChecked() ? 1.0 : 0.0;
    update();
}

void SwitchWidget::nextCheckState() {
    QAbstractButton::nextCheckState();
}

void SwitchWidget::startAnimation(bool checked) {
    _animation->stop();
    _animation->setStartValue(_position);
    _animation->setEndValue(checked ? 1.0 : 0.0);
    _animation->start();
}

} // namespace Linea::UI
