// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Separator — thin horizontal line widget with a style-driven color.
 *
 * Paints a single 1 device-pixel line, snapped to the device pixel grid so
 * it stays crisp on high-DPI displays. The line color is taken from the
 * widget's palette (WindowText role), so it can be set from the stylesheet
 * with the standard `color` property. Opacity is controlled separately via
 * the `qproperty-opacity` property (0.0–1.0).
 */

#ifndef LINEA_UI_SEPARATOR_H
#define LINEA_UI_SEPARATOR_H

#include <QWidget>

namespace Linea::UI {

class Separator : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit Separator(QWidget* parent = nullptr);
    ~Separator() override = default;

    qreal opacity() const { return _opacity; }
    void setOpacity(qreal opacity);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qreal _opacity = 1.0;
};

} // namespace Linea::UI

#endif // LINEA_UI_SEPARATOR_H
