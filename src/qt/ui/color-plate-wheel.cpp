// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * WheelColorPlate — hue/saturation disc with the value channel fixed. Also
 * holds the polar geometry helpers shared with the ring plate.
 */

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "color-plate-shapes.h"
#include "colors/spaces/base.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using namespace Inkscape::Colors;
using namespace Inkscape::Colors::Space;

// ---------------------------------------------------------------------------
// shared polar geometry
// ---------------------------------------------------------------------------

double sanitizeChannel(double value) {
    // broken conversions (OKLCH etc.) can yield NaN/Inf — std::clamp passes
    // NaN straight through, so sanitize before clamping
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

QRectF polarSquare(const QRectF& area) {
    const double sz = std::min(area.width(), area.height());
    return {area.center().x() - sz / 2, area.center().y() - sz / 2, sz, sz};
}

// maps a position to polar coordinates, clamping onto the nearest pickable
// point — clicks past the colored area pick the nearest edge color
QPointF polarFromScreen(const QRectF& active, const QPointF& pt) {
    const double half = std::min(active.width(), active.height()) / 2;
    double cx = (pt.x() - active.center().x()) / half;
    double cy = (pt.y() - active.center().y()) / half;
    const double dist = std::hypot(cx, cy);
    // ring mode needs interior coords for the triangle — only clamp
    // points past the outer edge
    if (dist > 1.0) {
        cx /= dist;
        cy /= dist;
    }
    return {cx, cy};
}

QPointF polarToScreen(const QRectF& active, const QPointF& local) {
    const double half = std::min(active.width(), active.height()) / 2;
    return {active.center().x() + local.x() * half, active.center().y() + local.y() * half};
}

QPointF polarPoint(double hue, double radius) {
    hue = sanitizeChannel(hue);
    radius = sanitizeChannel(radius);
    const double angle = hue * 2 * M_PI - M_PI;
    return {std::sin(angle) * radius, std::cos(angle) * radius};
}

double polarAngle(const QPointF& local) {
    return (std::atan2(local.x(), local.y()) + M_PI) / (2 * M_PI);
}

// ---------------------------------------------------------------------------
// plate image
// ---------------------------------------------------------------------------

namespace {

// Build the disc wheel image
QImage buildWheelPlate(int resolution, const Color& base) {
    int radius = resolution / 2;
    int size = radius * 2 + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    double limit = radius;
    // paint a couple of pixels past the rim, giving the clip's
    // antialiased edge extra margin
    double rsqr = std::pow(1.0 + 2.0 / radius, 2);
    Color color = base;
    for (int iy = -radius; iy <= radius; ++iy) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(iy + radius));
        double y = iy / limit;
        for (int ix = -radius; ix <= radius; ++ix) {
            double x = ix / limit;
            if (x * x + y * y > rsqr) continue;
            double dist = std::hypot(x, y);
            double angle = (std::atan2(x, y) + M_PI) / (2 * M_PI);
            color.set(PLATE_HUE, angle);
            color.set(PLATE_SAT, dist);
            line[ix + radius] = QRgb(color.toARGB());
        }
    }
    return img;
}

} // namespace

// ---------------------------------------------------------------------------

WheelColorPlate::WheelColorPlate(Type source, Type space, const PlateGamut& gamut, QWidget* parent)
    : ColorPlate(source, space, PLATE_VAL, gamut.surface, parent) {}

void WheelColorPlate::colorChanged() {
    // if value changes, color wheel needs to be rebuilt
    if (channelDelta(plateSpace(), PLATE_VAL, _built_val, color()[PLATE_VAL]) > 0.005) {
        _plate = {};
    }
    _marker = polarPoint(color()[PLATE_HUE], color()[PLATE_SAT]);
}

Color WheelColorPlate::colorAt(const QPointF& pos) const {
    const auto local = polarFromScreen(activeArea(), pos);
    auto c = color();
    c.set(PLATE_HUE, polarAngle(local));
    c.set(PLATE_SAT, std::hypot(local.x(), local.y()));
    return c;
}

std::vector<QPointF> WheelColorPlate::markers() const {
    if (!_marker) return {};
    return {polarToScreen(activeArea(), *_marker)};
}

void WheelColorPlate::rebuildPlate() {
    constexpr int resolution = 256;
    _built_val = color()[PLATE_VAL];
    _plate = buildWheelPlate(resolution, color());
    // the builder produces sRGB bytes; convert once, at build time
    _plate.setColorSpace(QColorSpace(QColorSpace::SRgb));
    toSurface(_plate);
}

void WheelColorPlate::paintPlate(QPainter& p) {
    const auto a = area();
    if (_plate.isNull()) rebuildPlate();

    const auto square = polarSquare(a);
    QPainterPath clip;
    clip.addEllipse(square);
    p.setClipPath(clip);

    // draw the plate image scaled into its area — the disc always maps to a
    // square centered inside the rect, never stretched into an oval;
    // round the target rect outwards so the image edge never lands inside the
    // clip — toRect() can pull it up to a pixel in on one side, leaving an uncovered jagged strip
    p.drawImage(square.toAlignedRect(), _plate);

    p.setClipping(false);

    // border
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    drawStandardBorder(p, a, dark, PLATE_CORNER_RADIUS, devicePixelRatio(), true);
}

} // namespace Linea::UI
