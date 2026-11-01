// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RectColorPlate — rectangular color plate. In an unbounded plate space the
 * image is clipped to the requested gamut and narrower gamuts can be outlined.
 */

#include <QPainter>
#include <algorithm>
#include <cassert>

#include "color-plate-shapes.h"
#include "colors/spaces/base.h"
#include "colors/spaces/linear-rgb.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using namespace Inkscape::Colors;
using namespace Inkscape::Colors::Space;

namespace {

// Trace the margin == 0 contour of a per-pixel margin grid with marching
// squares (linear interpolation on the cell edges), into a path in
// normalized plate coordinates (x right, y down).
QPainterPath gamutBoundary(const std::vector<float>& grid, int size) {
    QPainterPath path;
    const double res = size - 1;
    auto at = [&](int x, int y) { return grid[y * size + x]; };
    // fraction along an edge where the margin crosses zero
    auto crossing = [](double a, double b) { return a / (a - b); };
    for (int y = 0; y < size - 1; ++y) {
        for (int x = 0; x < size - 1; ++x) {
            const double tl = at(x, y), tr = at(x + 1, y), br = at(x + 1, y + 1), bl = at(x, y + 1);
            int mask = (tl >= 0 ? 1 : 0) | (tr >= 0 ? 2 : 0) | (br >= 0 ? 4 : 0) | (bl >= 0 ? 8 : 0);
            if (mask == 0 || mask == 15) continue;
            const QPointF top((x + crossing(tl, tr)) / res, y / res);
            const QPointF right((x + 1) / res, (y + crossing(tr, br)) / res);
            const QPointF bottom((x + crossing(bl, br)) / res, (y + 1) / res);
            const QPointF left(x / res, (y + crossing(tl, bl)) / res);
            // diagonal masks are saddles: which corners the segments cut off
            // depends on the cell center, and the test differs between the two
            // diagonals, so they cannot be folded into each other
            if (mask == 5 || mask == 10) {
                if (((tl + tr + br + bl) / 4 >= 0) == (mask == 5)) {
                    path.moveTo(top); path.lineTo(right);
                    path.moveTo(left); path.lineTo(bottom);
                } else {
                    path.moveTo(left); path.lineTo(top);
                    path.moveTo(right); path.lineTo(bottom);
                }
                continue;
            }
            // complementary masks trace the same boundary
            if (mask > 7) mask = 15 - mask;
            switch (mask) {
                case 1: // tl inside
                    path.moveTo(left); path.lineTo(top);
                    break;
                case 2: // tr inside
                    path.moveTo(top); path.lineTo(right);
                    break;
                case 3: // top half inside
                    path.moveTo(left); path.lineTo(right);
                    break;
                case 4: // br inside
                    path.moveTo(right); path.lineTo(bottom);
                    break;
                case 6: // right half inside
                    path.moveTo(top); path.lineTo(bottom);
                    break;
                default: // 7: only bl outside
                    path.moveTo(bottom); path.lineTo(left);
                    break;
            }
        }
    }
    return path;
}

} // namespace

RectColorPlate::RectColorPlate(Type source, const PlateSpec& spec, const PlateGamut& gamut, QWidget* parent)
    : ColorPlate(source, spec.rect, spec.axes.fixed, gamut.surface, parent)
    , _axes(spec.axes)
    , _bounded(!spec.gamut)
    , _gamut(gamut) {}

void RectColorPlate::colorChanged() {
    // rebuild when the fixed channel moved — it is held constant across the plate
    if (channelDelta(plateSpace(), _axes.fixed, _built_fixed, color()[_axes.fixed]) > 0.005) {
        _plate = {};
    }
    _marker = QPointF(sanitizeChannel(color()[_axes.x]), 1.0 - sanitizeChannel(color()[_axes.y]));
}

Color RectColorPlate::colorAt(const QPointF& pos) const {
    const auto active = activeArea();
    // clamp to the active area and normalize to 0..1 — clicks past the
    // colored area pick the nearest edge color
    const double nx = (std::clamp(pos.x(), active.left(), active.right()) - active.left()) / active.width();
    const double ny = (std::clamp(pos.y(), active.top(), active.bottom()) - active.top()) / active.height();
    auto c = color();
    c.set(_axes.x, nx);
    c.set(_axes.y, 1.0 - ny);
    return c;
}

std::vector<QPointF> RectColorPlate::markers() const {
    if (!_marker) return {};
    const auto active = activeArea();
    return {{active.left() + _marker->x() * active.width(),
             active.top() + _marker->y() * active.height()}};
}

// Build a rectangular plate image
void RectColorPlate::rebuildPlate() {
    constexpr int resolution = 256;
    _built_fixed = color()[_axes.fixed];
    if (!_bounded) {
        rebuildGamutPlate(resolution);
        return;
    }
    const int size = resolution + 1;
    _plate = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
    auto c = color();
    c.addOpacity();
    for (int iy = 0; iy <= resolution; ++iy) {
        auto line = reinterpret_cast<QRgb*>(_plate.scanLine(iy));
        c.set(_axes.y, 1.0 - iy / double(resolution));
        for (int ix = 0; ix <= resolution; ++ix) {
            c.set(_axes.x, ix / double(resolution));
            line[ix] = QRgb(c.toARGB());
        }
    }
    // the builders produce sRGB bytes; convert once, at build time
    _plate.setColorSpace(QColorSpace(QColorSpace::SRgb));
    toSurface(_plate);
}

// Unbounded plate space: pixels whose color leaves the requested gamut are
// transparent — the plate shows the gamut boundary instead of silently
// clipping. In the same pass, record margin grids for the requested outline
// gamuts and trace their boundaries.
void RectColorPlate::rebuildGamutPlate(int resolution) {
    const int size = resolution + 1;
    _plate = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
    _plate.fill(Qt::transparent);
    _srgb_boundary = {};
    _p3_boundary = {};

    const auto space = color().getSpace();
    const auto& base = color().getValues();
    // reused 3-vector of plate values, channel roles restored per pixel
    std::vector<double> io(base.begin(), base.begin() + 3);
    // reused conversion buffer — spaceToProfile rewrites it in place
    std::vector<double> vals(io.size());

    const bool want_srgb = _gamut.srgb_line && _gamut.gamut != Gamut::sRGB;
    const bool want_p3 = _gamut.p3_line && _gamut.gamut == Gamut::Rec2020;
    std::vector<float> srgb_grid, p3_grid;
    if (want_srgb) srgb_grid.resize(size * size);
    if (want_p3) p3_grid.resize(size * size);

    // pixels are encoded with the sRGB TRC below — a Rec.2020 surface would
    // need the BT.2020 curve instead, and gamutColorSpace() already tags Bt2020
    assert(_gamut.surface != Gamut::Rec2020);

    std::array<double, 3> lin;
    std::array<double, 3> out;
    for (int iy = 0; iy <= resolution; ++iy) {
        auto line = reinterpret_cast<QRgb*>(_plate.scanLine(iy));
        io[_axes.y] = 1.0 - iy / double(resolution);
        for (int ix = 0; ix <= resolution; ++ix) {
            io[_axes.x] = ix / double(resolution);
            std::copy(io.begin(), io.end(), vals.begin());
            // yields unclamped encoded sRGB for internal spaces
            space->spaceToProfile(vals);
            LinearRGB::fromRGB(vals.data(), lin.data());
            const double margin = gamut_margin(lin, _gamut.gamut);
            // margins of the outline gamuts are recorded for every pixel —
            // outside-target pixels are out of the narrower gamuts too
            if (want_srgb) srgb_grid[iy * size + ix] = gamut_margin(lin, Gamut::sRGB);
            if (want_p3) p3_grid[iy * size + ix] = gamut_margin(lin, Gamut::DisplayP3);
            if (margin < 0) continue;
            // inside the gamut: pixels are computed directly in the surface
            // space — encode (Display P3 uses the sRGB transfer curve), then
            // clamp; the curve is monotonic, so this equals clamping linear
            out = linear_srgb_to(lin, _gamut.surface);
            LinearRGB::toRGB(out.data(), out.data());
            line[ix] = qRgba(qRound(std::clamp(out[0], 0.0, 1.0) * 255),
                             qRound(std::clamp(out[1], 0.0, 1.0) * 255),
                             qRound(std::clamp(out[2], 0.0, 1.0) * 255), 255);
        }
    }
    if (want_srgb) _srgb_boundary = gamutBoundary(srgb_grid, size);
    if (want_p3) _p3_boundary = gamutBoundary(p3_grid, size);
    // already in the surface space — toSurface is a no-op on this image
    _plate.setColorSpace(gamutColorSpace(_gamut.surface));
}

void RectColorPlate::paintPlate(QPainter& p) {
    const auto a = area();
    if (_plate.isNull()) rebuildPlate();

    QPainterPath clip;
    clip.addRoundedRect(a, PLATE_CORNER_RADIUS, PLATE_CORNER_RADIUS);
    p.setClipPath(clip);

    // round the target rect outwards so the image edge never lands inside the
    // clip — toRect() can pull it up to a pixel in on one side, leaving an uncovered jagged strip
    p.drawImage(a.toAlignedRect(), _plate);

    // gamut outlines drawn over the plate, inside the clip
    if (!_srgb_boundary.isEmpty() || !_p3_boundary.isEmpty()) {
        QPen pen(QColor(255, 255, 255, 204));
        pen.setWidthF(1.0);
        pen.setCosmetic(true);
        p.setPen(pen);
        QTransform tr;
        tr.translate(a.left(), a.top());
        tr.scale(a.width(), a.height());
        if (!_srgb_boundary.isEmpty()) p.drawPath(tr.map(_srgb_boundary));
        if (!_p3_boundary.isEmpty()) p.drawPath(tr.map(_p3_boundary));
    }

    p.setClipping(false);

    // border
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    drawStandardBorder(p, a, dark, PLATE_CORNER_RADIUS, devicePixelRatio(), false);
}

} // namespace Linea::UI
