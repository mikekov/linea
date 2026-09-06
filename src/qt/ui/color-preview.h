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
    explicit ColorPreview(std::uint32_t rgba = 0, QWidget* parent = nullptr);

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

    // checkerboard tile size in pixels
    void setCheckerboardTileSize(unsigned size);

    // fill / stroke role indicators
    void setFill(bool on);
    void setStroke(bool on);

    // gradient preview (overrides solid color)
    struct GradientStop { double offset, r, g, b, a; };
    void setGradient(std::vector<GradientStop> stops);

    // image preview (overrides solid color and gradient; e.g. for patterns)
    void setImage(QImage image);

    // QSize sizeHint() const override { return {16, 16}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawCheckers(QPainter& p, const QRectF& rect) const;
    void drawGradient(QPainter& p, const QRectF& rect, double radius) const;
    void drawSolidColor(QPainter& p, const QRectF& rect, double radius) const;
    void drawIndicators(QPainter& p, const QRectF& rect) const;
    void drawFrame(QPainter& p, const QRectF& rect) const;

    std::uint32_t _rgba = 0;
    Style    _style    = Simple;
    Indicator _indicator = None;
    int      _radius   = -1;
    bool     _frame    = false;
    bool     _isFill   = false;
    bool     _isStroke = false;
    unsigned _checkerTile = 6;
    std::vector<GradientStop> _gradient;
    QImage _image;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PREVIEW_H
