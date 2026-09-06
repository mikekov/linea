// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Qt drawing utilities
 */

#ifndef LINEA_QT_UTIL_DRAWING_UTILS_H
#define LINEA_QT_UTIL_DRAWING_UTILS_H

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>
#include <functional>
#include <cstdint>

#include "2geom/int-rect.h"
#include "2geom/affine.h"

class FontInstance;
class SPGradient;

namespace Linea::UI {

/**
 * Calculate luminance of a QColor from its RGB in range 0 to 1 inclusive.
 * Ported from GTK version's get_luminance.
 * This uses the perceived brightness formula given at: https://www.w3.org/TR/AERT/#color-contrast
 */
double getLuminance(const QColor& color);

/**
 * Create a checkerboard pattern pixmap for use as a QBrush.
 * The pattern adapts its contrast to the base color's brightness,
 * making it suitable for both light and dark themes.
 * @param base The base color (typically the window/background color)
 * @param tile Tile size in pixels (default 3)
 * @return A 2×tile pixmap suitable for QBrush tiling
 */
QPixmap checkerPattern(const QColor& base, int tile = 3);

/**
 * Get the standard border color for dark/light theme.
 */
QColor getStandardBorderColor(bool darkTheme);

/**
 * Draw a border shape with relief effect using a callback for path drawing.
 * The callback should return a QPainterPath and modify the rect for the next iteration.
 */
void drawBorderShape(QPainter& p, QRectF rect, const QColor& color, double deviceScale,
                     std::function<QPainterPath(QRectF&, int)> drawPath);

/**
 * Draw a standard border with relief effect.
 */
void drawStandardBorder(QPainter& p, const QRectF& rect, bool darkTheme, double radius, double deviceScale, bool circular = false, bool inwards = true);

/**
 * Draw the GTK-style page drop shadow around a rectangular page.
 */
void drawPageShadow(QPainter& p, const QRectF& rect, double size = 6.0,
                    const QColor& color = QColor(0, 0, 0), double alpha = 0.30);

/**
 * Determine whether the background palette is dark using Qt's palette, so we
 * can pick an appropriate thumb stroke color.
 */
bool isDarkPalette(const QWidget& widget);

/**
 * Create a size-independent QLinearGradient from an SPGradient.
 * Coordinates are in object-bounding mode and color stops are normalized.
 */
QLinearGradient toQLinearGradient(SPGradient* gradient);

/**
 * Parameters for drawing font glyphs with QPainter.
 * Ported from Cairo-based draw_glyph_params.
 */
struct draw_glyph_params {
    // font to use
    FontInstance* font = nullptr;
    // draw at requested size (or 0 for auto-fit)
    double font_size = 0;
    // index of the glyph to draw
    std::uint32_t glyph_index = 0;
    // painter to draw to
    QPainter* painter = nullptr;
    // available area
    Geom::IntRect rect;
    // colors to use
    QColor glyph_color;
    QColor line_color;
    QColor background_color;
    // draw baseline, ascender and descender lines
    bool draw_metrics = false;
    // fill background with color
    bool draw_background = false;
};

/**
 * Draw requested glyph using QPainter.
 * Ported from Cairo-based draw_glyph function.
 */
void drawGlyph(const draw_glyph_params& params);

/**
 * Convert Geom::PathVector to QPainterPath for Qt rendering.
 * Ported from Cairo feed_pathvector_to_cairo.
 * @param pathv The path vector to convert
 * @param trans Optional affine transform (defaults to identity)
 */
QPainterPath pathVectorToQPainterPath(const Geom::PathVector& pathv, const Geom::Affine& trans = Geom::identity());

/**
 * Convert single Geom::Path to QPainterPath.
 * Ported from Cairo feed_path_to_cairo.
 */
QPainterPath pathToQPainterPath(const Geom::Path& path, const Geom::Affine& trans = Geom::identity());

/**
 * Convert single Geom::Curve to QPainterPath operations.
 * Ported from Cairo feed_curve_to_cairo.
 */
void curveToQPainterPath(QPainterPath& result, const Geom::Curve& c, const Geom::Affine& trans = Geom::identity());

} // namespace Linea::UI

#endif // LINEA_QT_UTIL_DRAWING_UTILS_H
