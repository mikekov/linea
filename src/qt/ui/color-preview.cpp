// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPreview — color preview swatch widget.
 */

#include "color-preview.h"

#include <algorithm>
#include <cmath>
#include <QIcon>
#include <QMouseEvent>
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

// perceptual lightness of a color, 0..1 — WCAG relative luminance (linearized
// sRGB, Rec.709 weights) passed through the CIE L* curve, so 0.5 ≈ mid-gray
static double lightness(const QColor& c) {
    auto lin = [](float v) {
        return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
    };
    const double y = 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
    constexpr double e = 216.0 / 24389.0; // (6/29)^3
    return (y > e ? 116.0 * std::cbrt(y) - 16.0 : (24389.0 / 27.0) * y) / 100.0;
}

// ---------------------------------------------------------------------------

ColorPreview::ColorPreview(QWidget* parent, std::uint32_t rgba)
    : QWidget(parent)
    , _rgba(rgba)
{
    setObjectName("ColorPreview");
}

void ColorPreview::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        _pressed = true;
        if (_pipette) update();
    }
    QWidget::mousePressEvent(event);
}

void ColorPreview::mouseReleaseEvent(QMouseEvent* event) {
    if (_pressed) {
        _pressed = false;
        if (_pipette) update();
    }
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        Q_EMIT clicked();
    }
    QWidget::mouseReleaseEvent(event);
}

void ColorPreview::enterEvent(QEnterEvent* event) {
    if (_pipette) update();
    QWidget::enterEvent(event);
}

void ColorPreview::leaveEvent(QEvent* event) {
    if (_pressed || _pipette) {
        _pressed = false;
        update();
    }
    QWidget::leaveEvent(event);
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

void ColorPreview::setWarning(bool on) {
    if (_warning == on) return;
    _warning = on;
    update();
}

void ColorPreview::setShowPipette(bool show) {
    if (_pipette == show) return;
    _pipette = show;
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
        // pick a contrasting ink from the fill's perceptual lightness
        QColor ink = (lightness(fill) > 0.5) ? Qt::black : Qt::white;

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

void ColorPreview::drawOutline(QPainter& p, const QRectF& rect, double radius) const {
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    QColor borderColor = !dark ? QColor(0, 0, 0, 25) : QColor(0xff, 0xff, 0xff, 25);

    QPainterPath border;
    addRoundedRect(border, rect.adjusted(0.5, 0.5, -0.5, -0.5), std::max(0.0, radius - 0.5));

    p.save();
    p.setPen(QPen(borderColor, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(border);
    p.restore();
}

// lightness of the content under a centered badge: mean gradient stop or the
// solid color, opacity ignored; images/mixed fall back to the window color
double ColorPreview::badgeLightness() const {
    if (!_gradient.empty()) {
        double sum = 0.0;
        for (const auto& s : _gradient) {
            sum += lightness(QColor::fromRgbF(s.r, s.g, s.b));
        }
        return sum / _gradient.size();
    }
    if (_image.isNull() && !(_indicator & MixedContent)) {
        return lightness(fromRgba32(_rgba));
    }
    return palette().color(QPalette::Window).lightnessF();
}

void ColorPreview::drawBadge(QPainter& p, const QRectF& rect, const QIcon& darkIcon, const QIcon& lightIcon) const {
    constexpr int size = 16;
    const auto& icon = (badgeLightness() > 0.50) ? darkIcon : lightIcon;
    auto pm = icon.pixmap(QSize(size, size), devicePixelRatioF());
    p.drawPixmap(rect.center() - QPointF(size / 2.0, size / 2.0), pm);
}

void ColorPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF rect(0, 0, width(), height());
    double radius = (_radius >= 0) ? _radius : (_style == Simple ? 0.0 : 3.0);

    // click feedback: the color surface recedes inside its frame
    QRectF content = rect;
    double contentRadius = radius;
    if (_pipette && _pressed) {
        content = content.adjusted(1, 1, -1, -1);
        contentRadius = std::max(0.0, contentRadius - 1.0);
    }

    // clip to final rect
    QPainterPath clip;
    addRoundedRect(clip, content, contentRadius);
    p.setClipPath(clip);

    if (_indicator & MixedContent) {
        // Mixed selection
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(content, Qt::AlignCenter, tr("Mixed"));
    } else if (!_image.isNull()) {
        double ratio = _image.devicePixelRatio();
        QRectF src(0, 0,
                   std::min((double)_image.width(),  content.width()  * ratio),
                   std::min((double)_image.height(), content.height() * ratio));
        QRectF dst(content.left(), content.top(), src.width() / ratio, src.height() / ratio);
        p.drawImage(dst, _image, src);
    } else if (!_gradient.empty()) {
        drawGradient(p, content, contentRadius);
    } else {
        drawSolidColor(p, content, contentRadius);
    }

    p.setClipping(false);

    drawIndicators(p, content);

    if (_style == Outlined) {
        drawOutline(p, content, contentRadius);
    } else if (_frame) {
        drawFrame(p, content);
    }

    if (_pipette && underMouse()) {
        static const QIcon dark(":/icons/pipette-black"), light(":/icons/pipette-white");
        drawBadge(p, rect, dark, light);
    } else if (_warning) {
        static const QIcon dark(":/icons/warning-black"), light(":/icons/warning-white");
        drawBadge(p, rect, dark, light);
    }
}

} // namespace Linea::UI
