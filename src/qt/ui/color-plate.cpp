// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPlate — color plate widget.
 */

#include "color-plate.h"

#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

#include "colors/color.h"
#include "colors/spaces/base.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

// ---------------------------------------------------------------------------
// helper functions
// ---------------------------------------------------------------------------

namespace {

void setColorHelper(Color& color, int channel1, int channel2, double x, double y, bool disc) {
    if (disc) {
        double dist  = std::hypot(x, y);
        double angle = (std::atan2(x, y) + M_PI) / (2 * M_PI);
        color.set(channel1, angle);
        color.set(channel2, dist);
    } else {
        color.set(channel1, x);
        color.set(channel2, 1.0 - y);
    }
}

QPointF getColorCoordinates(double val1, double val2, bool circular) {
    val1 = std::clamp(val1, 0.0, 1.0);
    val2 = std::clamp(val2, 0.0, 1.0);
    if (circular) {
        double angle = val1 * 2 * M_PI - M_PI;
        return {std::sin(angle) * val2, std::cos(angle) * val2};
    }
    return {val1, 1.0 - val2};
}

// Build a rectangular plate image
QImage buildColorPlate(int resolution, const Color& base, int ch1, int ch2) {
    int size = resolution + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    auto color = base;
    color.addOpacity();
    for (int iy = 0; iy <= resolution; ++iy) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(iy));
        double y = iy / double(resolution);
        color.set(ch2, 1.0 - y);
        for (int ix = 0; ix <= resolution; ++ix) {
            color.set(ch1, ix / double(resolution));
            uint32_t argb = color.toARGB();
            line[ix] = QRgb(argb);
        }
    }
    return img;
}

// Build a circular (disc) wheel image
QImage buildColorWheel(int resolution, const Color& base, int ch1, int ch2) {
    int radius = resolution / 2;
    int size   = radius * 2 + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    double limit = radius;
    double rsqr  = std::pow(1.0 + 1.0 / radius, 2);
    Color color  = base;
    for (int iy = -radius; iy <= radius; ++iy) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(iy + radius));
        double y   = iy / limit;
        for (int ix = -radius; ix <= radius; ++ix) {
            double x = ix / limit;
            if (x * x + y * y > rsqr) continue;
            setColorHelper(color, ch1, ch2, x, y, true);
            line[ix + radius] = QRgb(color.toARGB());
        }
    }
    return img;
}

} // namespace

// ---------------------------------------------------------------------------

ColorPlate::ColorPlate(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ColorPlate::setBaseColor(Color color, int fixedChannel, int varChannel1, int varChannel2) {
    color.setOpacity(1);
    bool needsRebuild = (_baseColor.getSpace() != color.getSpace()) ||
                        (std::fabs(_fixedChannelVal - color[fixedChannel]) > 0.005) ||
                        (_channel1 != varChannel1) ||
                        (_channel2 != varChannel2);
    _baseColor = std::move(color);
    if (needsRebuild) {
        _fixedChannelVal = _baseColor[fixedChannel];
        _channel1 = varChannel1;
        _channel2 = varChannel2;
        _plate = {};
        update();
    }
}

void ColorPlate::setDisc(bool disc) {
    if (_disc == disc) return;
    _disc  = disc;
    _plate = {};
    update();
}

void ColorPlate::setColor(const Color& color) {
    // Base implementation: convert to current plate space, update indicator only.
    // FastColorPlate in the factory overrides this with the proper fixed/var channel logic.
    auto plateType = _baseColor.getSpace() ? _baseColor.getSpace()->getType() : Space::Type::RGB;
    auto copy = color.converted(plateType);
    if (copy) {
        moveIndicatorTo(*copy);
    }
}

sigc::connection ColorPlate::connectColorChanged(sigc::slot<void(const Color&)> cb) {
    return _signal_color_changed.connect(std::move(cb));
}

void ColorPlate::moveIndicatorTo(const Color& color) {
    auto pt = getColorCoordinates(color[_channel1], color[_channel2], _disc);
    if (_indicator && *_indicator == pt) return;
    _indicator = pt;
    update();
}

// ---------------------------------------------------------------------------
// geometry helpers
// ---------------------------------------------------------------------------

QRectF ColorPlate::getArea() const {
    QRectF r(0, 0, width(), height());
    return r.adjusted(_padding, _padding, -_padding, -_padding);
}

QRectF ColorPlate::getActiveArea() const {
    return getArea().adjusted(1, 1, -1, -1);
}

QPointF ColorPlate::screenToLocal(const QRectF& active, QPointF pt, bool* inside) const {
    if (inside) *inside = active.contains(pt);
    // clamp to active area
    double x = std::clamp(pt.x(), active.left(),  active.right());
    double y = std::clamp(pt.y(), active.top(),   active.bottom());
    // normalize to 0..1
    double nx = (x - active.left()) / active.width();
    double ny = (y - active.top())  / active.height();

    if (_disc) {
        double min = std::min(active.width(), active.height());
        double sx  = min / active.width();
        double sy  = min / active.height();
        double cx  = (nx * 2 - 1) / sx;
        double cy  = (ny * 2 - 1) / sy;
        double dist = std::hypot(cx, cy);
        if (dist > 1.0) {
            cx /= dist; cy /= dist;
            if (inside) *inside = false;
        }
        return {cx, cy};
    }
    return {nx, ny};
}

QPointF ColorPlate::localToScreen(const QRectF& active, QPointF local) const {
    if (_disc) {
        double min = std::min(active.width(), active.height());
        double sx  = min / active.width();
        double sy  = min / active.height();
        double nx  = (local.x() * sx + 1) / 2;
        double ny  = (local.y() * sy + 1) / 2;
        return {active.left() + nx * active.width(),
                active.top()  + ny * active.height()};
    }
    return {active.left() + local.x() * active.width(),
            active.top()  + local.y() * active.height()};
}

Color ColorPlate::getColorAt(const QPointF& local) const {
    auto color = _baseColor;
    setColorHelper(color, _channel1, _channel2, local.x(), local.y(), _disc);
    return color;
}

void ColorPlate::fireColorChanged() {
    if (!_indicator) return;
    auto color = getColorAt(*_indicator);
    _signal_color_changed.emit(color);
}

// ---------------------------------------------------------------------------
// plate rendering
// ---------------------------------------------------------------------------

void ColorPlate::rebuildPlate() {
    constexpr int resolution = 128;
    if (_disc) {
        _plate = buildColorWheel(resolution, _baseColor, _channel1, _channel2);
    } else {
        _plate = buildColorPlate(resolution, _baseColor, _channel1, _channel2);
    }
}

void ColorPlate::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF area = getArea();
    if (area.width() <= 0 || area.height() <= 0) return;

    if (_plate.isNull()) rebuildPlate();

    // clip
    QPainterPath clip;
    if (_disc) {
        double sz = std::min(area.width(), area.height());
        QRectF circle(area.center().x() - sz/2, area.center().y() - sz/2, sz, sz);
        clip.addEllipse(circle);
    } else {
        clip.addRoundedRect(area, _radius, _radius);
    }
    p.setClipPath(clip);

    // draw the plate image scaled into the area
    p.drawImage(area.toRect(), _plate);

    p.setClipping(false);

    // border
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    drawStandardBorder(p, area, dark, _radius, devicePixelRatio(), _disc);

    // indicator dot
    if (_indicator) {
        QRectF active = getActiveArea();
        QPointF pt    = localToScreen(active, *_indicator);
        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pt, 5.0, 5.0);
        p.setPen(QPen(Qt::black, 1));
        p.drawEllipse(pt, 5.0, 5.0);
        p.restore();
    }
}

// ---------------------------------------------------------------------------
// mouse events
// ---------------------------------------------------------------------------

void ColorPlate::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);

    QRectF active = getActiveArea();
    bool inside = false;
    auto local = screenToLocal(active, e->position(), &inside);
    if (inside) {
        _indicator = local;
        _drag = true;
        update();
        fireColorChanged();
        e->accept();
    } else {
        _indicator = {};
        _drag = false;
    }
}

void ColorPlate::mouseMoveEvent(QMouseEvent* e) {
    if (!_drag || !(e->buttons() & Qt::LeftButton)) return QWidget::mouseMoveEvent(e);

    QRectF active = getActiveArea();
    _indicator = screenToLocal(active, e->position());
    update();
    fireColorChanged();
    e->accept();
}

void ColorPlate::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return QWidget::mouseReleaseEvent(e);

    _drag = false;
    e->accept();
}

} // namespace Linea::UI
