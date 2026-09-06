// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPlate — color plate (rectangular or circular color picker widget).
 */

#ifndef LINEA_UI_COLOR_PLATE_H
#define LINEA_UI_COLOR_PLATE_H

#include <optional>
#include <QImage>
#include <QPointF>
#include <QWidget>
#include <sigc++/signal.h>

#include "color-wheel.h"
#include "colors/color.h"
#include "colors/spaces/enum.h"

namespace Linea::UI {

/**
 * A rectangular or circular color plate widget.
 *
 * Renders a 2D color grid where one channel is fixed and two channels vary
 * along the X/Y axes. The user can click/drag to pick a color.
 */
class ColorPlate : public QWidget, public ColorWheel {
    Q_OBJECT
public:
    explicit ColorPlate(QWidget* parent = nullptr);

    // Set which color space channels are used: fixed channel value plus two varying channels.
    void setBaseColor(Inkscape::Colors::Color color, int fixedChannel, int varChannel1, int varChannel2);

    // Set disc (circular) vs rectangular mode.
    void setDisc(bool disc);
    bool isDisc() const { return _disc; }

    // ColorWheel interface
    void setColor(const Inkscape::Colors::Color& color) override;
    sigc::connection connectColorChanged(sigc::slot<void(const Inkscape::Colors::Color&)> cb) override;
    QWidget* getWidget() override { return this; }

    // Move the on-plate indicator to match a given color.
    void moveIndicatorTo(const Inkscape::Colors::Color& color);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return {215, 215}; }

private:
    QRectF getArea() const;
    QRectF getActiveArea() const;
    QPointF screenToLocal(const QRectF& active, QPointF pt, bool* inside = nullptr) const;
    QPointF localToScreen(const QRectF& active, QPointF local) const;
    Inkscape::Colors::Color getColorAt(const QPointF& local) const;
    void fireColorChanged();
    void rebuildPlate();

    int _padding = 4;
    double _radius = 4.0;
    bool _disc = true;
    bool _drag = false;

    Inkscape::Colors::Color _baseColor{Inkscape::Colors::Space::Type::RGB, {0, 0, 0}};
    double _fixedChannelVal = -1.0;
    int _channel1 = 1;
    int _channel2 = 2;

    QImage _plate;
    std::optional<QPointF> _indicator;

    sigc::signal<void(const Inkscape::Colors::Color&)> _signal_color_changed;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PLATE_H
