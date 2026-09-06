// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Qt drawing utilities
 */

#include "drawing-utils.h"

#include <QPainterPath>
#include <QWidget>
#include <algorithm>
#include <cmath>

#include "2geom/pathvector.h"
#include "2geom/path.h"
#include "2geom/curve.h"
#include "2geom/bezier-curve.h"
#include "2geom/elliptical-arc.h"
#include "2geom/sbasis-to-bezier.h"
#include "2geom/affine.h"
#include "libnrtype/font-instance.h"
#include "colors/color.h"
#include "object/sp-gradient.h"
#include "object/sp-stop.h"

namespace Linea::UI {

double getLuminance(const QColor& color) {
    // This formula is recommended at https://www.w3.org/TR/AERT/#color-contrast
    return 0.299 * color.redF() + 0.587 * color.greenF() + 0.114 * color.blueF();
}

// Create a checkerboard pattern with the given base color (background) and tile size
QPixmap checkerPattern(const QColor& base, int tile) {
    constexpr int contrast = 60;
    int v = base.value();
    int newV = (v < 128) ? qMin(255, v + contrast) : qMax(0, v - contrast);
    QColor other = QColor::fromHsv(base.hsvHue(), base.hsvSaturation(), newV, base.alpha());
    QPixmap p(tile * 2, tile * 2);
    p.fill(base);
    QPainter pt(&p);
    pt.fillRect(0, 0, tile, tile, other);
    pt.fillRect(tile, tile, tile, tile, other);
    pt.end();
    return p;
}

QColor getStandardBorderColor(bool darkTheme) {
    return darkTheme ? QColor(255, 255, 255, 64)   // 0.25 alpha * 255 = 64
                     : QColor(0,   0,   0,   64);   // 0.25 alpha * 255 = 64
}

void drawBorderShape(QPainter& p, QRectF rect, const QColor& color, double deviceScale,
                     std::function<QPainterPath(QRectF&, int)> drawPath) {
    if (rect.width() < 1 || rect.height() < 1) return;

    // eliminate one pixel overhang
    double pix = 1.0 / deviceScale;
    rect = QRectF(rect.x(), rect.y(), rect.width() - pix, rect.height() - pix);

    p.save();
    // operate on physical pixels
    p.scale(1.0 / deviceScale, 1.0 / deviceScale);
    // align 1.0 wide stroke to pixel grid
    p.translate(0.5, 0.5);
    p.setPen(QPen(color, 1.0));
    p.setBrush(::Qt::NoBrush);

    // shadow depth
    const int steps = 3 * deviceScale;
    double alpha = color.alphaF();
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // rect in physical pixels
    rect = QRectF(rect.topLeft() * deviceScale, rect.bottomRight() * deviceScale);

    for (int i = 0; i < steps; ++i) {
        // Callback creates the path and modifies rect
        QPainterPath path = drawPath(rect, i);
        // Now stroke with current alpha
        QColor strokeColor = color;
        strokeColor.setAlphaF(alpha);
        p.setPen(QPen(strokeColor, 1.0));
        p.drawPath(path);
        alpha *= 0.5;
    }
    p.restore();
}

void drawBorder(QPainter& p, QRectF startRect, double radius, const QColor& color, double deviceScale, bool circular, bool inwards) {
    radius *= deviceScale;

    drawBorderShape(p, startRect, color, deviceScale, [&](QRectF& currentRect, int step) {
        QPainterPath path;
        if (circular) {
            double circleRadius = std::min(currentRect.width(), currentRect.height()) / 2.0;
            QPointF center = currentRect.center();
            path.addEllipse(center, circleRadius, circleRadius);
        } else {
            double currentRadius = inwards ? radius - step : radius + step;
            path.addRoundedRect(currentRect, currentRadius, currentRadius);
        }

        if (inwards) {
            currentRect = currentRect.adjusted(1, 1, -1, -1);
        } else {
            currentRect = currentRect.adjusted(-1, -1, 1, 1);
        }

        return path;
    });
}

void drawStandardBorder(QPainter& p, const QRectF& rect, bool darkTheme, double radius, double deviceScale, bool circular, bool inwards) {
    QColor color = getStandardBorderColor(darkTheme);
    drawBorder(p, rect, radius, color, deviceScale, circular, inwards);
}

void drawPageShadow(QPainter& p, const QRectF& rect, double size, const QColor& color, double alpha) {
    if (rect.isEmpty() || size <= 0 || alpha <= 0) return;

    auto topLeft = rect.topLeft();
    auto topRight = rect.topRight();
    auto bottomRight = rect.bottomRight();
    auto bottomLeft = rect.bottomLeft();
    auto half = size / 2.0;
    auto shadowColor = color;
    // Draw a fake drop shadow built from gradients, matching
    // ink_cairo_draw_drop_shadow().
    const int stops = 15; // number of gradient stops; stops make the shadow nonlinear
    constexpr double steepness = 4.0; // controls the shadow drop-off
    auto denominator = std::exp(steepness) - 1.0;

    // Eight gradients total: four sides and four corners.
    QLinearGradient top(0, topLeft.y() + half, 0, topLeft.y() - half);
    QLinearGradient right(topRight.x(), 0, topRight.x() + size, 0);
    QLinearGradient bottom(0, bottomRight.y(), 0, bottomRight.y() + size);
    QLinearGradient left(topLeft.x() + half, 0, topLeft.x() - half, 0);
    QRadialGradient bottomRightCorner(bottomRight, size);
    QRadialGradient topRightCorner(QPointF(topRight.x(), topRight.y() + half), size);
    QRadialGradient bottomLeftCorner(QPointF(bottomLeft.x() + half, bottomLeft.y()), size);
    QRadialGradient topLeftCorner(topLeft, half);

    // Use the easing function from the Cairo implementation:
    // (exp(a * (1 - t)) - 1) / (exp(a) - 1).
    // It grows from 0 to 1 and gives the shadow a long, smooth tail.
    for (int i = 0; i <= stops; ++i) {
        auto position = static_cast<double>(i) / stops;
        auto t = 1.0 - position;
        auto stopAlpha = (std::exp(steepness * t) - 1.0) / denominator * alpha;
        shadowColor.setAlphaF(stopAlpha);
        top.setColorAt(position, shadowColor);
        right.setColorAt(position, shadowColor);
        bottom.setColorAt(position, shadowColor);
        left.setColorAt(position, shadowColor);
        bottomRightCorner.setColorAt(position, shadowColor);
        topRightCorner.setColorAt(position, shadowColor);
        bottomLeftCorner.setColorAt(position, shadowColor);
        if (position >= 0.5) {
            topLeftCorner.setColorAt(2.0 * (position - 0.5), shadowColor);
        }
    }

    p.save();
    // Shadow at the top (faint).
    p.fillRect(QRectF(topLeft.x(), topLeft.y() - half, std::max(rect.width(), 0.0), half), top);
    // Right and bottom sides carry the visible drop shadow.
    p.fillRect(QRectF(topRight.x(), topRight.y() + half, size, std::max(rect.height() - half, 0.0)), right);
    p.fillRect(QRectF(topLeft.x() + half, bottomRight.y(), std::max(rect.width() - half, 0.0), size), bottom);
    // Left side is faint, matching the Cairo implementation.
    p.fillRect(QRectF(topLeft.x() - half, topLeft.y(), half, std::max(rect.height(), 0.0)), left);
    // Bottom and top corner gradients.
    p.fillRect(QRectF(bottomRight.x(), bottomRight.y(), size, size), bottomRightCorner);
    p.fillRect(QRectF(bottomLeft.x() - half, bottomLeft.y(), std::min(size, rect.width() + half), size), bottomLeftCorner);
    p.fillRect(QRectF(topRight.x(), topRight.y() - half, size, std::min(size, rect.height() + half)), topRightCorner);
    // The top-left corner is only a slice of the shadow because the rest is hidden beneath the page.
    p.fillRect(QRectF(topLeft.x() - half, topLeft.y() - half, half, half), topLeftCorner);
    p.restore();
}

/**
 * Determine whether the background palette is dark using Qt's palette, so we
 * can pick an appropriate thumb stroke color.
 */
bool isDarkPalette(const QWidget& widget) {
    auto bg = widget.palette().color(QPalette::Window);
    return getLuminance(bg) < 0.5;
}

/**
 * Convert single Geom::Curve to QPainterPath operations.
 * Ported from Cairo feed_curve_to_cairo.
 */
void curveToQPainterPath(QPainterPath& result, const Geom::Curve& c, const Geom::Affine& trans) {
    // Determine curve order
    unsigned order = 0;
    if (auto* b = dynamic_cast<Geom::BezierCurve const*>(&c)) {
        order = b->order();
    }

    switch (order) {
    case 1: // Line segment
    {
        auto end = c.finalPoint() * trans;
        result.lineTo(end.x(), end.y());
    }
    break;
    case 2: // Quadratic Bezier - degree elevate to cubic
    {
        auto* quad = static_cast<Geom::QuadraticBezier const*>(&c);
        std::array<Geom::Point, 3> points;
        for (int i = 0; i < 3; i++) {
            points[i] = quad->controlPoint(i) * trans;
        }
        // Degree-elevate to cubic Bezier
        Geom::Point b1 = points[0] + (2./3) * (points[1] - points[0]);
        Geom::Point b2 = b1 + (1./3) * (points[2] - points[0]);
        result.cubicTo(b1.x(), b1.y(), b2.x(), b2.y(), points[2].x(), points[2].y());
    }
    break;
    case 3: // Cubic Bezier
    {
        auto* cubic = static_cast<Geom::CubicBezier const*>(&c);
        auto c1 = cubic->controlPoint(1) * trans;
        auto c2 = cubic->controlPoint(2) * trans;
        auto end = cubic->finalPoint() * trans;
        result.cubicTo(c1.x(), c1.y(), c2.x(), c2.y(), end.x(), end.y());
    }
    break;
    default:
    {
        // Handle elliptical arcs and other curve types
        if (auto* arc = dynamic_cast<Geom::EllipticalArc const*>(&c)) {
            if (arc->isChord()) {
                auto end = arc->finalPoint() * trans;
                result.lineTo(end.x(), end.y());
            } else {
                // Convert arc to cubic bezier approximation
                const int segments = 20;
                for (int i = 1; i <= segments; ++i) {
                    double t = static_cast<double>(i) / segments;
                    auto point = arc->pointAt(t) * trans;
                    result.lineTo(point.x(), point.y());
                }
            }
        } else {
            // Generic curve (SBasis, etc.) - convert to cubic bezier path
            try {
                Geom::Path sbasis_path = Geom::cubicbezierpath_from_sbasis(c.toSBasis(), 0.1);
                for (const auto& iter : sbasis_path) {
                    curveToQPainterPath(result, iter, trans);
                }
            } catch (...) {
                // Fallback to point sampling if conversion fails
                const int segments = 20;
                for (int i = 1; i <= segments; ++i) {
                    double t = static_cast<double>(i) / segments;
                    auto point = c.pointAt(t) * trans;
                    result.lineTo(point.x(), point.y());
                }
            }
        }
    }
    break;
    }
}

/**
 * Convert single Geom::Path to QPainterPath.
 * Ported from Cairo feed_path_to_cairo.
 */
QPainterPath pathToQPainterPath(const Geom::Path& path, const Geom::Affine& trans) {
    QPainterPath result;

    if (path.empty()) return result;

    Geom::Point p0 = path.initialPoint() * trans;
    result.moveTo(p0.x(), p0.y());

    for (Geom::Path::const_iterator it = path.begin(); it != path.end_open(); ++it) {
        curveToQPainterPath(result, *it, trans);
    }

    if (path.closed()) {
        result.closeSubpath();
    }

    return result;
}

/**
 * Convert Geom::PathVector to QPainterPath for Qt rendering.
 * Ported from Cairo feed_pathvector_to_cairo.
 */
QPainterPath pathVectorToQPainterPath(const Geom::PathVector& pathv, const Geom::Affine& trans) {
    QPainterPath result;

    if (pathv.empty()) return result;

    for (const auto& path : pathv) {
        QPainterPath path_result = pathToQPainterPath(path, trans);
        result.addPath(path_result);
    }

    return result;
}

void drawGlyph(const draw_glyph_params& params) {
    if (params.rect.hasZeroArea()) return;

    auto font = params.font;
    auto& rect = params.rect;
    auto* painter = params.painter;

    auto glyph = font->LoadGlyph(params.glyph_index);
    if (!glyph) {
        // bitmap font? svg font?
        //todo
        return;
    }

    double font_size = params.font_size;
    if (font_size == 0) {
        // try to find optimal size, so we don't clip any glyph
        auto max_size = glyph->bbox_exact.dimensions();
        if (params.draw_metrics) {
            // account for space for vertical lines showing character width
            max_size.x() = std::max(glyph->h_advance, max_size.x());
        }
        max_size.y() = std::max(font->GetMaxAscent() + font->GetMaxDescent(), max_size.y());
        // inflate the size by 10% to leave margins
        max_size *= 1.1;

        // limit the size to at most 70% of available area to leave space around
        // and stop glyphs from dominating the drawing;
        // the aim is to try and keep the same scale from glyph to glyph as far as possible
        auto size_limit = 0.70;
        Geom::Point size{1, 1};
        if (max_size.y() > 0) {
            size.y() = std::min(1.0 / max_size.y(), size_limit);
        }
        if (max_size.x() > 0) {
            size.x() = std::min(1.0 / max_size.x(), size_limit);
        }
        font_size = std::min(size.x() * rect.width(), size.y() * rect.height());
    }
    // shift all glyphs vertically by the same amount so the baseline doesn't fluctuate
    // when switching from one glyph to another
    auto shift = (rect.height() - font_size * (font->GetMaxAscent() - font->GetMaxDescent())) / 2;

    // check if the glyph fits; some fonts will need this correction, but it should be infrequent
    auto top = shift + glyph->bbox_exact.bottom() * font_size;
    auto bottom = shift + glyph->bbox_exact.top() * font_size;
    if (top >= rect.height()) {
        shift -= top - rect.height();
    }
    else if (bottom < 0) {
        shift += -bottom;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(QRect(rect.left(), rect.top(), rect.width(), rect.height()));

    if (params.draw_background) {
        auto& bg = params.background_color;
        painter->fillRect(QRect(rect.left(), rect.top(), rect.width(), rect.height()), bg);
    }

    // Transform for glyph drawing
    painter->translate(rect.left(), rect.bottom() - shift);
    painter->scale(font_size, -font_size);
    auto w = static_cast<double>(rect.width()) / font_size;
    auto midpoint = glyph->bbox_exact.midpoint().x();
    auto center = w / 2;

    if (params.draw_metrics) {
        auto& color = params.line_color;
        painter->setPen(QPen(color, 1.0 / font_size));

        std::array lines = {
            font->GetBaselines()[SP_CSS_BASELINE_AUTO],
            font->GetTypoAscent(),
            -font->GetTypoDescent()
        };
        for (double y : lines) {
            painter->drawLine(QPointF(0, y), QPointF(w, y));
        }

        auto adv = glyph->h_advance / 2;
        std::array vert = {
            center - adv,
            center + adv,
        };
        for (double x : vert) {
            painter->drawLine(QPointF(x, -1), QPointF(x, +1));
        }
    }

    painter->translate(center - midpoint, 0);

    // Convert pathvector to QPainterPath and draw
    QPainterPath glyphPath = pathVectorToQPainterPath(glyph->pathvector);
    auto& fg = params.glyph_color;
    painter->setPen(Qt::NoPen);
    painter->setBrush(fg);
    painter->drawPath(glyphPath);

    painter->restore();
}

QLinearGradient toQLinearGradient(SPGradient* gradient) {
    QLinearGradient grad(0, 0, 1, 0);
    grad.setCoordinateMode(QGradient::ObjectBoundingMode);

    if (!gradient) {
        return grad;
    }

    // Follow the href chain to the gradient that actually has stops.
    auto vector = gradient->getVector();
    if (!vector) {
        return grad;
    }

    switch (vector->getSpread()) {
        case SP_GRADIENT_SPREAD_REFLECT:
            grad.setSpread(QGradient::ReflectSpread);
            break;
        case SP_GRADIENT_SPREAD_REPEAT:
            grad.setSpread(QGradient::RepeatSpread);
            break;
        case SP_GRADIENT_SPREAD_PAD:
        default:
            grad.setSpread(QGradient::PadSpread);
            break;
    }

    for (auto stop = vector->getFirstStop(); stop; stop = stop->getNextStop()) {
        auto rgba = QColor::fromRgba(stop->getColor().toARGB());
        grad.setColorAt(stop->offset, rgba);
    }

    return grad;
}

} // namespace Linea::UI
