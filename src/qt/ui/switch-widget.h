// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SwitchWidget — a toggle switch control with animated knob.
 */

#ifndef LINEA_UI_SWITCH_WIDGET_H
#define LINEA_UI_SWITCH_WIDGET_H

#include <QAbstractButton>

class QPropertyAnimation;

namespace Linea::UI {

/**
 * A toggle switch widget with a pill-shaped track and sliding knob.
 *
 * Colors are taken from the widget palette:
 *   - Inactive track: palette(button)
 *   - Active track:   palette(highlight)
 *   - Knob:           palette(base)
 *
 * Size is configurable via QSS (min-width, min-height).
 *
 * Inherits QAbstractButton, so checked state and toggled(bool) signal
 * are available out of the box.
 */
class SwitchWidget : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)

public:
    explicit SwitchWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override;

    qreal position() const { return _position; }
    void setPosition(qreal pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void checkStateSet() override;
    void nextCheckState() override;

private:
    void startAnimation(bool checked);

    qreal _position = 0.0;
    QPropertyAnimation* _animation = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_SWITCH_WIDGET_H
