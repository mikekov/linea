// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PaintIndicator — combined fill / stroke color indicator.
 *
 * Shows two overlapping squares representing the current fill and stroke
 * colors. One square is drawn in front of the other; the front square can be
 * selected. Either square can show a "no paint" diagonal stripe.
 *
 * A single outline color is used for both squares so it can be tied to the
 * active theme. The widget desaturates all colors when disabled.
 */

#ifndef LINEA_UI_PAINT_INDICATOR_H
#define LINEA_UI_PAINT_INDICATOR_H

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <variant>

class QMouseEvent;

namespace Linea::UI {

class PaintIndicator : public QWidget {
    Q_OBJECT

public:
    /// Which square is drawn on top (also identifies a logical part).
    enum class Part { Fill, Stroke };

    explicit PaintIndicator(QWidget* parent = nullptr);

    /// Set a solid color. @p part selects the fill or stroke square.
    void setColor(const QColor& color, Part part);
    /// Set a linear gradient. @p part selects the fill or stroke square.
    void setGradient(const QLinearGradient& gradient, Part part);
    /// Set a pattern image. @p part selects the fill or stroke square.
    void setPattern(const QImage& pattern, Part part);
    /// Set the "no paint" diagonal red stripe. @p part selects the fill or stroke square.
    void setNone(Part part);
    /// Set the indeterminate/mixed state. @p part selects the fill or stroke square.
    void setIndeterminate(Part part);
    /// Set the inherited state. @p part selects the fill or stroke square.
    void setInherited(Part part);
    // Set a solid color which represents a swatch
    void setSwatchColor(const QColor& color, Part part);

    /// Choose whether the fill or stroke square is rendered in front.
    void setTopSquare(Part top);
    Part topSquare() const { return _top; }

    /// Set the outline color used for both squares. If invalid, the widget
    /// falls back to the palette's window text color, which tracks the theme.
    void setOutlineColor(const QColor& color);

    /// Returns the size of the indicator area (the fill or stroke square).
    QRect indicatorSize() const;

Q_SIGNALS:
    /// Emitted when the user clicks on a square. @p part is the square that
    /// was hit (front square wins if the click falls in the overlap region).
    void clicked(Part part, const QPoint& pos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    struct None {};
    struct Indeterminate {};
    struct Inherited {};
    struct SwatchColor { QColor color; };
    using Paint = std::variant<None, QColor, SwatchColor, QLinearGradient, QPixmap, Indeterminate, Inherited>;

    void drawSquare(QPainter& p, const QRect& square, const Paint& paint, bool isStroke, bool hideCorner) const;
    void drawSwatchIndicator(QPainter& p, const QRect& square, const QPainterPath& clip) const;

    struct Layout {
        QRect fill;
        QRect stroke;
    };
    Layout layout() const;
    static QColor toGray(const QColor& color);

    QColor effectiveOutline() const;

    Paint _fill = QColor(Qt::black);
    Paint _stroke = QColor(Qt::white);
    QColor _outlineColor;
    Part _top = Part::Stroke;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAINT_INDICATOR_H
