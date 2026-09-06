// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPreview — color preview swatch widget.
 */

#include "color-preview.h"

#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>

#include "qt/util/drawing-utils.h"

namespace Linea::UI {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static void addRoundedRect(QPainterPath& path, const QRectF& r, double radius) {
    if (radius <= 0.0) {
        path.addRect(r);
    } else {
        path.addRoundedRect(r, radius, radius);
    }
}

static QColor fromRgba32(std::uint32_t rgba) {
    return QColor(
        (rgba >> 24) & 0xff,
        (rgba >> 16) & 0xff,
        (rgba >>  8) & 0xff,
        (rgba      ) & 0xff);
}

// ---------------------------------------------------------------------------

ColorPreview::ColorPreview(std::uint32_t rgba, QWidget* parent)
    : QWidget(parent)
    , _rgba(rgba)
{
    setObjectName("ColorPreview");
}

void ColorPreview::setRgba32(std::uint32_t rgba) {
    _rgba = rgba;
    _gradient.clear();
    update();
}

void ColorPreview::setStyle(Style style) {
    _style = style;
    update();
}

void ColorPreview::setIndicator(Indicator indicator) {
    _indicator = indicator;
    update();
}

void ColorPreview::setFrame(bool frame) {
    _frame = frame;
    update();
}

void ColorPreview::setBorderRadius(int radius) {
    _radius = radius;
    update();
}

void ColorPreview::setCheckerboardTileSize(unsigned size) {
    _checkerTile = size;
    update();
}

void ColorPreview::setFill(bool on) {
    _isFill = on;
    update();
}

void ColorPreview::setStroke(bool on) {
    _isStroke = on;
    update();
}

void ColorPreview::setGradient(std::vector<GradientStop> stops) {
    _gradient = std::move(stops);
    _image = {};
    _rgba = 0;
    update();
}

void ColorPreview::setImage(QImage image) {
    _image = std::move(image);
    _gradient.clear();
    _rgba = 0;
    update();
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------

void ColorPreview::drawCheckers(QPainter& p, const QRectF& rect) const {
    QBrush brush(checkerPattern(palette().color(QPalette::Window), static_cast<int>(_checkerTile)));
    p.setBrushOrigin(rect.topLeft().toPoint());
    p.fillRect(rect, brush);
}

void ColorPreview::drawGradient(QPainter& p, const QRectF& rect, double radius) const {
    QPainterPath clip;
    addRoundedRect(clip, rect, radius);
    p.setClipPath(clip);

    // checkerboard behind gradient
    drawCheckers(p, rect);

    // gradient overlay
    QLinearGradient grad(rect.left(), 0, rect.right(), 0);
    for (auto& s : _gradient) {
        grad.setColorAt(s.offset, QColor::fromRgbF(s.r, s.g, s.b, s.a));
    }
    p.fillRect(rect, grad);
    p.setClipping(false);
}

void ColorPreview::drawSolidColor(QPainter& p, const QRectF& rect, double radius) const {
    QColor full = fromRgba32(_rgba);
    QColor solid(full.red(), full.green(), full.blue(), 255);
    double alpha = full.alphaF();

    double halfW = rect.width() / 2.0;

    // left half — solid color (no alpha)
    QPainterPath leftPath;
    if (radius > 0.0) {
        QRectF bigRect(rect.left(), rect.top(), rect.width(), rect.height());
        // left-rounded rectangle: manually build
        leftPath.moveTo(rect.left() + halfW, rect.top());
        leftPath.lineTo(rect.left() + halfW, rect.bottom());
        leftPath.arcTo(QRectF(rect.left(), rect.bottom() - 2*radius, 2*radius, 2*radius), 270, -90);
        leftPath.arcTo(QRectF(rect.left(), rect.top(), 2*radius, 2*radius), 180, -90);
        leftPath.closeSubpath();
    } else {
        leftPath.addRect(QRectF(rect.left(), rect.top(), halfW, rect.height()));
    }
    p.fillPath(leftPath, solid);

    // right half — alpha-blended over checkerboard
    QPainterPath rightPath;
    if (radius > 0.0) {
        rightPath.moveTo(rect.left() + halfW, rect.top());
        rightPath.arcTo(QRectF(rect.right() - 2*radius, rect.top(), 2*radius, 2*radius), 90, -90);
        rightPath.arcTo(QRectF(rect.right() - 2*radius, rect.bottom() - 2*radius, 2*radius, 2*radius), 0, -90);
        rightPath.lineTo(rect.left() + halfW, rect.bottom());
        rightPath.closeSubpath();
    } else {
        rightPath.addRect(QRectF(rect.left() + halfW, rect.top(), halfW, rect.height()));
    }

    if (alpha < 1.0) {
        // clip to right half and draw checkerboard
        p.save();
        p.setClipPath(rightPath);
        drawCheckers(p, QRectF(rect.left() + halfW, rect.top(), halfW, rect.height()));
        p.restore();
    }
    p.fillPath(rightPath, full);
}

void ColorPreview::drawIndicators(QPainter& p, const QRectF& rect) const {
    if (_isFill || _isStroke) {
        QColor fill = fromRgba32(_rgba);
        // perceptual lightness — use simple luminance estimate
        double lum = 0.299 * fill.redF() + 0.587 * fill.greenF() + 0.114 * fill.blueF();
        QColor ink = (lum > 0.5) ? Qt::black : Qt::white;

        double minwh = std::min(rect.width(), rect.height());
        double cx = rect.left() + (rect.width() - minwh) / 2.0 + minwh / 2.0;
        double cy = rect.top()  + (rect.height() - minwh) / 2.0 + minwh / 2.0;
        double scale = minwh / 2.0;

        p.save();
        p.translate(cx, cy);
        p.scale(scale, scale);
        p.setBrush(ink);
        p.setPen(Qt::NoPen);

        if (_isFill) {
            p.drawEllipse(QPointF(0, 0), 0.35, 0.35);
        }
        if (_isStroke) {
            QPainterPath ring;
            ring.addEllipse(QPointF(0, 0), 0.65, 0.65);
            ring.addEllipse(QPointF(0, 0), 0.50, 0.50);
            ring.setFillRule(Qt::OddEvenFill);
            p.fillPath(ring, ink);
        }
        p.restore();
    }

    if (_indicator == None) return;

    constexpr double side = 6.5;
    constexpr double line = 1.5;
    double left   = rect.left();
    double top    = rect.top();

    p.save();
    p.setPen(Qt::NoPen);

    if (_indicator & Swatch) {
        // black corner triangle with white separator line
        QPolygonF white, black;
        white << QPointF(left, top + side) << QPointF(left, top + side - line)
              << QPointF(left + side - line, top) << QPointF(left + side, top);
        black << QPointF(left, top + side - line) << QPointF(left, top)
              << QPointF(left + side - line, top);
        p.setBrush(Qt::white);
        p.drawPolygon(white);
        p.setBrush(Qt::black);
        p.drawPolygon(black);
    } else if (_indicator & SpotColor) {
        QPolygonF bg;
        bg << QPointF(left, top) << QPointF(left, top + side) << QPointF(left + side, top);
        p.setBrush(Qt::white);
        p.drawPolygon(bg);
        p.setBrush(Qt::black);
        double r = 2;
        p.drawEllipse(QPointF(left + r, top + r), r, r);
    }

    if (_indicator & (LinearGradient | RadialGradient)) {
        double s = 3.0, h = s / 2.0;
        double minwh = std::min(rect.width(), rect.height());
        double w = minwh - 2*s - 2;
        double cx = std::round(rect.left() + rect.width() / 2.0);
        double cy = std::round(rect.top()  + rect.height() / 2.0);

        QPainterPath arrow;
        if (_indicator & LinearGradient) {
            QPointF p0(rect.left() + 1 + s + (rect.width() - minwh) / 2.0, cy);
            // arrow right → traverse → arrow left
            const double dx[] = {0,  -s,  s,  0,  w,  0,  s, -s,  0};
            const double dy[] = {h,  -h, -h,  h,  0,  h, -h, -h,  h};
            arrow.moveTo(p0);
            QPointF cur = p0;
            for (int i = 0; i < 9; ++i) { cur += QPointF(dx[i], dy[i]); arrow.lineTo(cur); }
        } else {
            QPointF p0(cx, rect.top() + 1 + s);
            double rcx = std::min(cx, cy);
            const double dx[] = {h,  -h, -h,  h,  0,  rcx-s-1,  0,  s, -s,  0,  -(rcx-s-1)};
            const double dy[] = {0,  -s,  s,  0,  cy-s-1,  0,  h, -h, -h,  h,  0};
            arrow.moveTo(p0);
            QPointF cur = p0;
            for (int i = 0; i < 11; ++i) { cur += QPointF(dx[i], dy[i]); arrow.lineTo(cur); }
        }
        arrow.closeSubpath();

        QPen strokePen;
        strokePen.setWidthF(2.0);
        strokePen.setMiterLimit(10);
        strokePen.setColor(Qt::white);
        p.setPen(strokePen);
        p.setBrush(Qt::black);
        p.drawPath(arrow);
        strokePen.setWidthF(1.0);
        strokePen.setColor(Qt::black);
        p.setPen(strokePen);
        p.drawPath(arrow);
    }

    p.restore();
}

void ColorPreview::drawFrame(QPainter& p, const QRectF& rect) const {
    p.save();
    // subtle dark overlay frame
    QPen pen(QColor(0, 0, 0, 18));
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    p.restore();
}

void ColorPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF rect(0, 0, width(), height());
    double radius = (_radius >= 0) ? _radius : (_style == Simple ? 0.0 : 2.0);
    bool dark = palette().color(QPalette::Window).lightness() < 128;

    if (_style == Outlined) {
        // outer outline
        QPainterPath outer;
        addRoundedRect(outer, rect, radius);
        QColor outlineColor = dark ? QColor(0xff, 0xff, 0xff, 0x5f) : QColor(0, 0, 0, 0x5f);
        p.fillPath(outer, outlineColor);
        rect = rect.adjusted(1, 1, -1, -1);
        radius = std::max(0.0, radius - 1.0);

        // inner border
        QPainterPath inner;
        addRoundedRect(inner, rect, radius);
        QColor borderColor = dark ? QColor(0, 0, 0, 255) : QColor(0xff, 0xff, 0xff, 255);
        p.fillPath(inner, borderColor);
        rect = rect.adjusted(1, 1, -1, -1);
        radius = std::max(0.0, radius - 1.0);
    }

    // clip to final rect
    QPainterPath clip;
    addRoundedRect(clip, rect, radius);
    p.setClipPath(clip);

    if (_indicator & MixedContent) {
        // Mixed selection
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(rect, Qt::AlignCenter, tr("Mixed"));
        p.setClipping(false);
        if (_style == Simple && _frame) drawFrame(p, rect);
        return;
    }

    if (!_image.isNull()) {
        double ratio = _image.devicePixelRatio();
        QRectF src(0, 0,
                   std::min((double)_image.width(),  rect.width()  * ratio),
                   std::min((double)_image.height(), rect.height() * ratio));
        QRectF dst(rect.left(), rect.top(), src.width() / ratio, src.height() / ratio);
        p.drawImage(dst, _image, src);
    } else if (!_gradient.empty()) {
        drawGradient(p, rect, radius);
    } else {
        drawSolidColor(p, rect, radius);
    }

    p.setClipping(false);

    drawIndicators(p, rect);

    if (_style == Simple && _frame) {
        drawFrame(p, rect);
    }
}

} // namespace Linea::UI
