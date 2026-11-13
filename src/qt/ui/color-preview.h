// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPreview — color preview swatch widget.
 *
 * Shows a split color patch (solid left half / alpha-blended right half over checkerboard).
 * Supports gradient stops, style (Simple / Outlined), optional border, and corner indicators
 * (Swatch, SpotColor, LinearGradient, RadialGradient).
 */

#ifndef LINEA_UI_COLOR_PREVIEW_H
#define LINEA_UI_COLOR_PREVIEW_H

#include <cstdint>
#include <vector>
#include <QImage>
#include <QWidget>

namespace Linea::UI {

class ColorPreview : public QWidget {
    Q_OBJECT
public:
    explicit ColorPreview(QWidget* parent = nullptr, std::uint32_t rgba = 0);

    // set preview color as RGBA32 (0xRRGGBBAA)
    void setRgba32(std::uint32_t rgba);

    // simple color patch vs outlined color patch (with contrasting border)
    enum Style { Simple, Outlined };
    void setStyle(Style style);
    Style style() const { return _style; }

    // corner indicators
    enum Indicator { None = 0, Swatch = 1, SpotColor = 2, LinearGradient = 4, RadialGradient = 8, MixedContent = 16 };
    void setIndicator(Indicator indicator);

    // subtle 1-px frame around Simple style
    void setFrame(bool frame);

    // corner radius; -1 = auto (0 for Simple, 2 for Outlined)
    void setBorderRadius(int radius);

    // edges kept square instead of rounded — for a swatch butt-joined to a
    // neighbouring control, e.g. an entry frame sharing one side
    void setFlatEdges(Qt::Edges edges);

    // checkerboard tile size in pixels
    void setCheckerboardTileSize(unsigned size);

    // persistent warning badge overlaid on the swatch (e.g. out-of-gamut);
    // the widget's own tooltip carries the message
    void setWarning(bool on);

    // pipette icon shown on mouse hover (replaces the warning badge while hovered)
    void setShowPipette(bool show);

    // fill / stroke role indicators
    void setFill(bool on);
    void setStroke(bool on);

    // gradient preview (overrides solid color)
    struct GradientStop { double offset, r, g, b, a; };
    void setGradient(std::vector<GradientStop> stops);

    // image preview (overrides solid color and gradient; e.g. for patterns)
    void setImage(QImage image);

    // QSize sizeHint() const override { return {16, 16}; }

Q_SIGNAL void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawCheckers(QPainter& p, const QRectF& rect) const;
    void drawGradient(QPainter& p, const QRectF& rect, double radiusL, double radiusR) const;
    void drawSolidColor(QPainter& p, const QRectF& rect, double radiusL, double radiusR) const;
    void drawIndicators(QPainter& p, const QRectF& rect) const;
    void drawFrame(QPainter& p, const QRectF& rect) const;
    void drawOutline(QPainter& p, const QRectF& rect, double radiusL, double radiusR) const;
    double badgeLightness() const;
    void drawBadge(QPainter& p, const QRectF& rect, const class QIcon& darkIcon, const class QIcon& lightIcon) const;

    std::uint32_t _rgba = 0;
    Style    _style    = Simple;
    Indicator _indicator = None;
    int      _radius   = -1;
    Qt::Edges _flat_edges{};
    bool     _frame    = false;
    bool     _isFill   = false;
    bool     _isStroke = false;
    bool     _warning  = false;
    bool     _pipette  = false;
    bool     _pressed  = false;
    unsigned _checkerTile = 6;
    std::vector<GradientStop> _gradient;
    QImage _image;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PREVIEW_H
