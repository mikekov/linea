// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Internal: the three ColorPlate shapes and the polar geometry shared by
 * wheel and ring. Not included outside the color-plate files.
 */

#ifndef LINEA_UI_COLOR_PLATE_SHAPES_H
#define LINEA_UI_COLOR_PLATE_SHAPES_H

#include <QColorSpace>
#include <QImage>
#include <QPainterPath>
#include <array>
#include <optional>

#include "color-plate.h"
#include "colors/spaces/components.h"

namespace Linea::UI {

// polar plate channel layout: hue around the circle, saturation along the
// radius, value driven by its slider
constexpr int PLATE_HUE = 0;
constexpr int PLATE_SAT = 1;
constexpr int PLATE_VAL = 2;

// radius of a rectangular plate's rounded corners
constexpr double PLATE_CORNER_RADIUS = 4.0;

// largest square centered inside 'area' — polar plates always map to a
// square, never stretch into an oval
QRectF polarSquare(const QRectF& area);
// widget position -> (-1..1) polar coordinates; points past the outer edge
// clamp onto the rim, interior points keep their radius (the ring needs
// interior coords for its triangle)
QPointF polarFromScreen(const QRectF& active, const QPointF& pt);
QPointF polarToScreen(const QRectF& active, const QPointF& local);
// hue/saturation -> polar point
QPointF polarPoint(double hue, double radius);
// polar point -> hue angle in 0..1
double polarAngle(const QPointF& local);
// broken conversions (OKLCH etc.) can yield NaN/Inf — std::clamp passes NaN
// straight through, so sanitize before clamping to 0..1
double sanitizeChannel(double value);
// Gamut -> QColorSpace; the single place this mapping lives
QColorSpace gamutColorSpace(Inkscape::Colors::Gamut gamut);

// Rectangular plate: two channels vary along the axes, one is held constant.
// In an unbounded space the plate is drawn clipped to the requested gamut.
class RectColorPlate : public ColorPlate {
public:
    RectColorPlate(Inkscape::Colors::Space::Type source, const Inkscape::Colors::Space::PlateSpec& spec,
                   const PlateGamut& gamut, QWidget* parent);

protected:
    void colorChanged() override;
    void paintPlate(QPainter& p) override;
    std::vector<QPointF> markers() const override;
    Inkscape::Colors::Color colorAt(const QPointF& pos) const override;

private:
    void rebuildPlate();
    // unbounded plate: per-pixel gamut clip plus margin grids for the
    // requested gamut outlines, traced with marching squares
    void rebuildGamutPlate(int resolution);

    Inkscape::Colors::Space::PlateAxes _axes;
    bool _bounded; // bounded plate spaces render raw; unbounded clip to the gamut
    PlateGamut _gamut;
    QImage _plate;
    double _built_fixed = -1.0; // fixed channel value the cached image was built with
    // gamut outlines in normalized plate coords (x right, y down), built with the image
    QPainterPath _srgb_boundary;
    QPainterPath _p3_boundary;
    std::optional<QPointF> _marker; // in normalized plate coords
};

// Hue wheel: hue around the disc, saturation along the radius, value fixed.
class WheelColorPlate : public ColorPlate {
public:
    WheelColorPlate(Inkscape::Colors::Space::Type source, Inkscape::Colors::Space::Type space,
                    const PlateGamut& gamut, QWidget* parent);

protected:
    void colorChanged() override;
    void paintPlate(QPainter& p) override;
    std::vector<QPointF> markers() const override;
    Inkscape::Colors::Color colorAt(const QPointF& pos) const override;

private:
    void rebuildPlate();

    QImage _plate;
    double _built_val = -1.0; // value channel the cached image was built with
    std::optional<QPointF> _marker; // in polar coords
};

// Hue ring: hue picks on the band, saturation/value picks inside the
// inscribed triangle tracking the hue angle.
class RingColorPlate : public ColorPlate {
public:
    RingColorPlate(Inkscape::Colors::Space::Type source, Inkscape::Colors::Space::Type space,
                   const PlateGamut& gamut, QWidget* parent);

protected:
    void colorChanged() override;
    void paintPlate(QPainter& p) override;
    std::vector<QPointF> markers() const override;
    void beginPick(const QPointF& pos) override;
    Inkscape::Colors::Color colorAt(const QPointF& pos) const override;
    void hideEvent(QHideEvent* event) override;

private:
    // triangle vertices in widget coordinates — same transform as the plate image
    std::array<QPointF, 3> triangleScreenVerts(const QRectF& area) const;

    QImage _ring;
    QImage _triangle;
    double _built_hue = -1.0; // hue the cached triangle was built with
    bool _dragRing = false;
    std::optional<QPointF> _ringMarker; // in polar coords
    std::optional<QPointF> _triMarker;  // in polar coords
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PLATE_SHAPES_H
