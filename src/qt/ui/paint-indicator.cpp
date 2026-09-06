// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PaintIndicator — combined fill / stroke color indicator.
 */

#include "paint-indicator.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <algorithm>

#include "qt/util/drawing-utils.h"

namespace Linea::UI {

namespace {

constexpr QSize preferredSize{30, 30};
constexpr QSize minSize{16, 16};
constexpr double squareRatio = 0.60; ///< each square is 60% of the shorter side
constexpr double offsetRatio = 0.25; ///< offset of the back square from the front one
constexpr double noneLine = 1.5;     ///< width of the "no paint" diagonal line in pixels
constexpr qreal cornerRadius = 2.0;  ///< corner radius for rounded square

} // namespace

PaintIndicator::PaintIndicator(QWidget* parent)
    : QWidget(parent) {
    setObjectName("PaintIndicator");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void PaintIndicator::setColor(const QColor& color, Part part) {
    (part == Part::Fill ? _fill : _stroke) = color;
    update();
}

void PaintIndicator::setSwatchColor(const QColor& color, Part part) {
    (part == Part::Fill ? _fill : _stroke) = SwatchColor{color};
    update();
}

void PaintIndicator::setGradient(const QLinearGradient& gradient, Part part) {
    QLinearGradient g(gradient);
    g.setCoordinateMode(QGradient::ObjectBoundingMode);
    (part == Part::Fill ? _fill : _stroke) = g;
    update();
}

void PaintIndicator::setPattern(const QImage& pattern, Part part) {
    (part == Part::Fill ? _fill : _stroke) = QPixmap::fromImage(pattern);
    update();
}

void PaintIndicator::setNone(Part part) {
    (part == Part::Fill ? _fill : _stroke) = None{};
    update();
}

void PaintIndicator::setIndeterminate(Part part) {
    (part == Part::Fill ? _fill : _stroke) = Indeterminate{};
    update();
}

void PaintIndicator::setInherited(Part part) {
    (part == Part::Fill ? _fill : _stroke) = Inherited{};
    update();
}

void PaintIndicator::setTopSquare(Part top) {
    _top = top;
    update();
}

void PaintIndicator::setOutlineColor(const QColor& color) {
    _outlineColor = color;
    update();
}

QSize PaintIndicator::sizeHint() const {
    return preferredSize;
}

QSize PaintIndicator::minimumSizeHint() const {
    return minSize;
}

PaintIndicator::Layout PaintIndicator::layout() const {
    QRect bounds = rect();
    int side = static_cast<int>(std::min(bounds.width(), bounds.height()) * squareRatio);
    int offset = qRound(side * offsetRatio);

    int x = bounds.left() + (bounds.width() - side) / 2;
    int y = bounds.top()  + (bounds.height() - side) / 2;
    QRect centered(x, y, side, side);

    return {centered.translated(-offset, -offset), centered.translated(offset - 1, offset - 1)};
}

void PaintIndicator::mousePressEvent(QMouseEvent* event) {
    if (!event) return;

    auto l = layout();
    QPoint pos = event->pos();
    bool onFill = l.fill.contains(pos);
    bool onStroke = l.stroke.contains(pos);

    if (onFill && onStroke) {
        Q_EMIT clicked(_top == Part::Fill ? Part::Fill : Part::Stroke, pos);
        event->accept();
    } else if (onFill) {
        Q_EMIT clicked(Part::Fill, pos);
        event->accept();
    } else if (onStroke) {
        Q_EMIT clicked(Part::Stroke, pos);
        event->accept();
    } else {
        event->ignore();
    }
}

void PaintIndicator::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto l = layout();

    // Draw the back square first.
    if (_top == Part::Stroke) {
        drawSquare(p, l.fill, _fill, false, true);
        drawSquare(p, l.stroke, _stroke, true, false);
    } else {
        drawSquare(p, l.stroke, _stroke, true, false);
        drawSquare(p, l.fill, _fill, false, false);
    }
}

void PaintIndicator::drawSquare(QPainter& p, const QRect& square, const Paint& paint, bool isStroke,
                                bool hideCorner) const {
    // A SwatchColor is drawn like a plain QColor with a small corner indicator.
    const QColor* colorPtr = std::get_if<QColor>(&paint);
    bool isSwatch = false;
    if (auto swatch = std::get_if<SwatchColor>(&paint)) {
        colorPtr = &swatch->color;
        isSwatch = true;
    }
    if (colorPtr && !colorPtr->isValid()) {
        return;
    }

    QPainterPath shape;
    shape.addRoundedRect(square, cornerRadius, cornerRadius);

    if (hideCorner) {
        int offset = qRound(square.width() * offsetRatio);
        QRect front = square.translated(2 * offset + 1, 2 * offset + 1);
        QPainterPath cut;
        cut.addRect(front);
        shape = shape.subtracted(cut);
    }

    QRectF inner;
    if (isStroke) {
        int margin = qRound(square.width() * 0.20);
        inner = square.adjusted(margin, margin, -margin, -margin);
        if (inner.isValid()) {
            shape.addRect(inner);
        }
        shape.setFillRule(Qt::OddEvenFill);
    }

    if (std::holds_alternative<None>(paint)) {
        QColor fill = isEnabled() ? QColor(0xff, 0xff, 0xff) : QColor(0x80, 0x80, 0x80);
        p.fillPath(shape, fill);
    } else if (colorPtr) {
        const QColor& color = *colorPtr;
        if (color.alphaF() < 1.0) {
            // semitransparent color - draw checkerboard pattern on one half of the square
            QRect left(square.x(), square.y(), square.width() / 2, square.height());
            QRect right(left.right() + 1, square.y(), square.width() - left.width(), square.height());

            QColor opaque = color.toRgb();
            opaque.setAlphaF(1.0);
            QColor opaqueFill = isEnabled() ? opaque : toGray(opaque);

            p.save();
            p.setClipPath(shape);
            p.fillRect(left, opaqueFill);

            QBrush brush(checkerPattern(palette().color(QPalette::Window)));
            p.setBrush(brush);
            p.setBrushOrigin(right.topLeft());
            p.fillRect(right, p.brush());
            p.fillRect(right, isEnabled() ? color : toGray(color));
            p.restore();
        } else {
            QColor fill = isEnabled() ? color : toGray(color);
            p.fillPath(shape, fill);
        }

        if (isSwatch) {
            drawSwatchIndicator(p, square, shape);
        }
    } else if (auto gradient = std::get_if<QLinearGradient>(&paint)) {
        p.save();
        p.setClipPath(shape);
        p.drawTiledPixmap(square, checkerPattern(palette().color(QPalette::Window)));
        p.restore();
        p.fillPath(shape, QBrush(*gradient));
    } else if (auto pattern = std::get_if<QPixmap>(&paint)) {
        p.save();
        p.setClipPath(shape);
        p.drawTiledPixmap(square, checkerPattern(palette().color(QPalette::Window)));
        p.drawTiledPixmap(square, *pattern);
        p.restore();
    } else if (std::holds_alternative<Indeterminate>(paint)) {
        QColor bg = palette().color(QPalette::Window);
        QColor stripe = (bg.value() < 128) ? bg.lighter(120) : bg.darker(120);
        p.fillPath(shape, bg);

        p.save();
        p.setClipPath(shape);
        p.setPen(QPen(stripe, 1.0));
        int spacing = 4;
        for (int i = -square.height(); i < square.width() + square.height(); i += spacing) {
            p.drawLine(square.x() + i, square.bottom(), square.x() + i + square.height(), square.top());
        }
        p.restore();
    } else if (std::holds_alternative<Inherited>(paint)) {
        p.save();
        p.setClipPath(shape);
        p.setPen(Qt::NoPen);
        QRectF rect = square;

        QPolygonF whiteHalf;
        whiteHalf << QPointF(rect.topLeft()) << QPointF(rect.topRight()) << QPointF(rect.bottomLeft());
        p.setBrush(Qt::white);
        p.drawPolygon(whiteHalf);

        QPolygonF blackHalf;
        blackHalf << QPointF(rect.bottomRight()) << QPointF(rect.topRight()) << QPointF(rect.bottomLeft());
        p.setBrush(Qt::black);
        p.drawPolygon(blackHalf);

        p.restore();
    }

    QPainterPath outline;
    outline.addRoundedRect(QRectF(square).adjusted(-0.5, -0.5, 0.5, 0.5), cornerRadius + 0.5, cornerRadius + 0.5);
    if (hideCorner) {
        int offset = qRound(square.width() * offsetRatio);
        QRect front = square.translated(2 * offset + 1, 2 * offset + 1);
        QPainterPath cut;
        cut.addRect(front);
        outline = outline.subtracted(cut);
    }

    auto outlineColor = effectiveOutline();
    if (std::holds_alternative<Indeterminate>(paint)) {
        outlineColor.setAlphaF(outlineColor.alphaF() * 0.2);
    }
    auto pen = QPen(outlineColor, 1.0);
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setMiterLimit(4.0);
    if (inner.isValid()) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(inner.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    if (std::holds_alternative<None>(paint)) {
        QColor slash = isEnabled() ? Qt::red : toGray(Qt::red);
        QPen linePen(slash, noneLine, Qt::SolidLine, Qt::RoundCap);
        p.setPen(linePen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(square.bottomLeft() + QPoint(1, 0), square.topRight() + QPoint(0, 1));
    }

    p.strokePath(outline, pen);
}

void PaintIndicator::drawSwatchIndicator(QPainter& p, const QRect& square, const QPainterPath& clip) const {
    p.save();
    p.setClipPath(clip);
    p.setPen(Qt::NoPen);

    QRectF tile(square);
    auto side = std::min(tile.width(), tile.height()) / 3.0;
    auto line = 2.0;
    auto left = tile.left();
    auto top = tile.top();

    QPainterPath separator;
    separator.moveTo(left, top + side);
    separator.lineTo(left, top + side - line);
    separator.lineTo(left + side - line, top);
    separator.lineTo(left + side, top);
    separator.closeSubpath();
    p.setBrush(QColor::fromHslF(0, 0, 1, 0.7));
    p.drawPath(separator);

    QPainterPath triangle;
    auto extra = 1.0; // a little bit less than a line thickness, so triangle overlaps separator
    triangle.moveTo(left, top + side - extra);
    triangle.lineTo(left, top);
    triangle.lineTo(left + side - extra, top);
    triangle.closeSubpath();
    p.setBrush(QColor::fromHslF(0, 0, 0.25));
    p.drawPath(triangle);

    p.restore();
}

QColor PaintIndicator::toGray(const QColor& color) {
    const int gray = qGray(color.rgba());
    return QColor(gray, gray, gray, color.alpha());
}

QColor PaintIndicator::effectiveOutline() const {
    auto dark = palette().color(QPalette::WindowText);
    dark.setAlphaF(dark.alphaF() * 0.8);
    QColor color = _outlineColor.isValid() ? _outlineColor : dark;
    if (!isEnabled()) {
        color = toGray(color);
    }
    return color;
}

void PaintIndicator::changeEvent(QEvent* event) {
    switch (event->type()) {
        case QEvent::EnabledChange:
        case QEvent::PaletteChange:
            update();
            break;
        default:
            break;
    }
    QWidget::changeEvent(event);
}

QRect PaintIndicator::indicatorSize() const {
    auto w = width() > 0 ? width() : sizeHint().width();
    auto h = height() > 0 ? height() : sizeHint().height();
    if (w <= 0 || h <= 0) {
        w = preferredSize.width();
        h = preferredSize.height();
    }
    return QRect(0, 0, qRound(w * squareRatio), qRound(h * squareRatio));
}

} // namespace Linea::UI
