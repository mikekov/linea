// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RingColorPlate — hue ring with an inscribed saturation/value triangle
 * tracking the hue angle.
 */

#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QPainterPath>
#include <algorithm>
#include <array>
#include <cmath>

#include "color-plate-shapes.h"
#include "colors/spaces/base.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using namespace Inkscape::Colors;
using namespace Inkscape::Colors::Space;

// inner radius of a ring plate, as a fraction of its outer radius
constexpr double RING_INNER_RATIO = 0.90;

// shift the ring marker along its radius, in pixels (+ outward, - inward)
constexpr double RING_MARKER_OFFSET = 1.0;

// shrink the inscribed triangle's circumradius by this many pixels
constexpr double TRIANGLE_INSET = 2.0;

// band-swell animation tick, ~60 fps like the slider thumb's
constexpr int SWELL_TICK_MS = 16;

namespace {

// Build a ring (annulus) wheel image — hue varies with the angle,
// saturation is pinned to maximum (a pure hue ring)
QImage buildColorRing(int resolution, const Color& base) {
    int radius = resolution / 2;
    int size = radius * 2 + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    double limit = radius;
    // paint a couple of pixels past both edges, giving the clip's
    // antialiased rim extra margin
    double const margin = 2.0 / radius;
    double outer_sqr = std::pow(1.0 + margin, 2);
    double inner_sqr = std::pow(RING_INNER_RATIO - margin - 1.0 / radius, 2);
    Color color = base;
    color.set(PLATE_SAT, 1.0);
    color.set(PLATE_VAL, 1.0); // ring is the hue rainbow at max value
    for (int iy = -radius; iy <= radius; ++iy) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(iy + radius));
        double y = iy / limit;
        for (int ix = -radius; ix <= radius; ++ix) {
            double x = ix / limit;
            double rr = x * x + y * y;
            if (rr > outer_sqr || rr < inner_sqr) continue;
            double angle = (std::atan2(x, y) + M_PI) / (2 * M_PI);
            color.set(PLATE_HUE, angle);
            line[ix + radius] = QRgb(color.toARGB());
        }
    }
    return img;
}

// Triangle inscribed in the ring's inner circle, tracking the hue angle:
// apex = pure hue (s=1, v=1), then black and white corners at ±120° —
// same layout as the GTK wheel's find_triangle_points
std::array<QPointF, 3> triangleVerts(double inner, double hue) {
    std::array<QPointF, 3> vs;
    const double a = hue * 2 * M_PI - M_PI; // same convention as polarPoint
    for (int i = 0; i < 3; ++i) {
        double ang = a + i * 2.0 * M_PI / 3.0;
        vs[i] = QPointF(std::sin(ang), std::cos(ang)) * inner;
    }
    return vs;
}

// triangle circumradius in local units: inner radius minus the pixel inset
// (local radius 1.0 == half the area's shortest side)
double triInnerRadius(const QRectF& area) {
    const double half = std::min(area.width(), area.height()) / 2;
    return half > 0 ? RING_INNER_RATIO - TRIANGLE_INSET / half : RING_INNER_RATIO;
}

// barycentric weights of p within triangle (a, b, c); any negative = outside
std::array<double, 3> barycentric(const QPointF& p, const QPointF& a, const QPointF& b, const QPointF& c) {
    double den = (b.y() - c.y()) * (a.x() - c.x()) + (c.x() - b.x()) * (a.y() - c.y());
    double wa = ((b.y() - c.y()) * (p.x() - c.x()) + (c.x() - b.x()) * (p.y() - c.y())) / den;
    double wb = ((c.y() - a.y()) * (p.x() - c.x()) + (a.x() - c.x()) * (p.y() - c.y())) / den;
    return {wa, wb, 1.0 - wa - wb};
}

// distance from p to the triangle's outline in local units; 0 when inside
double distanceToTriangle(const QPointF& p, const std::array<QPointF, 3>& vs) {
    const auto [wa, wb, ww] = barycentric(p, vs[0], vs[1], vs[2]);
    if (wa >= 0 && wb >= 0 && ww >= 0) return 0.0;

    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < 3; ++i) {
        const QPointF a = vs[i], ab = vs[(i + 1) % 3] - a;
        const double t = std::clamp(QPointF::dotProduct(p - a, ab) / QPointF::dotProduct(ab, ab), 0.0, 1.0);
        const QPointF d = p - (a + ab * t);
        best = std::min(best, std::hypot(d.x(), d.y()));
    }
    return best;
}

// Build the ring's inner triangle image. Corners are lerped in sRGB between
// the hue, black and white vertices — matching the GTK wheel's approach of
// interpolating corner colors instead of per-pixel channel conversion.
QImage buildColorTriangle(int resolution, const Color& base, double inner) {
    int radius = resolution / 2;
    int size = radius * 2 + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const auto [apex, black, white] = triangleVerts(inner, base[PLATE_HUE]);
    Color corner = base;
    corner.set(PLATE_SAT, 1.0);
    corner.set(PLATE_VAL, 1.0);
    const QRgb cApex = QRgb(corner.toARGB());
    const double rA = qRed(cApex), gA = qGreen(cApex), bA = qBlue(cApex);

    // cover the triangle bounds, plus a few pixels of margin so the
    // antialiased clip edge — and the 2px hover swell — has room to work
    const double eps = 4.0 / radius;
    const int x0 = std::clamp(static_cast<int>(std::floor((std::min({apex.x(), black.x(), white.x()}) - eps) * radius)) + radius, 0, size - 1);
    const int x1 = std::clamp(static_cast<int>(std::ceil((std::max({apex.x(), black.x(), white.x()}) + eps) * radius)) + radius, 0, size - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor((std::min({apex.y(), black.y(), white.y()}) - eps) * radius)) + radius, 0, size - 1);
    const int y1 = std::clamp(static_cast<int>(std::ceil((std::max({apex.y(), black.y(), white.y()}) + eps) * radius)) + radius, 0, size - 1);

    for (int iy = y0; iy <= y1; ++iy) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(iy));
        double y = (iy - radius) / double(radius);
        for (int ix = x0; ix <= x1; ++ix) {
            double x = (ix - radius) / double(radius);
            auto [wa, wb, ww] = barycentric(QPointF(x, y), apex, black, white);
            if (wa < -eps || wb < -eps || ww < -eps) continue;
            // clamp + renormalize: projects the pixel onto the triangle so the
            // margin pixels take the nearest edge color instead of fringe junk
            wa = std::clamp(wa, 0.0, 1.0);
            wb = std::clamp(wb, 0.0, 1.0);
            ww = std::clamp(ww, 0.0, 1.0);
            double wsum = wa + wb + ww;
            if (wsum <= 0.0) continue;
            wa /= wsum;
            ww /= wsum;
            // black corner contributes 0 per channel, white contributes 255
            int r = qRound(ww * 255 + wa * rA);
            int g = qRound(ww * 255 + wa * gA);
            int b = qRound(ww * 255 + wa * bA);
            line[ix] = qRgba(r, g, b, 255);
        }
    }
    return img;
}

} // namespace

// ---------------------------------------------------------------------------

RingColorPlate::RingColorPlate(Type source, Type space, const PlateGamut& gamut, QWidget* parent)
    : ColorPlate(source, space, PLATE_VAL, gamut.surface, parent) {
    // the base plate disables tracking; the ring wants hover feedback
    setMouseTracking(true);
    _anim_timer = new QTimer(this);
    _anim_timer->setInterval(SWELL_TICK_MS);
    connect(_anim_timer, &QTimer::timeout, this, &RingColorPlate::onSwellTick);
}

// would a press at pos grab the ring? inside the hole, whichever element
// is closer — the band's inner edge or the triangle outline
bool RingColorPlate::ringGrab(const QPointF& pos) const {
    const auto area = activeArea();
    const auto local = polarFromScreen(area, pos);
    const double r = std::hypot(local.x(), local.y());
    return r >= RING_INNER_RATIO ||
           RING_INNER_RATIO - r <
               distanceToTriangle(local, triangleVerts(grownTriRadius(area), color()[PLATE_HUE]));
}

// triangle circumradius incl. the hover swell, in local units — clip,
// border, hit test and picking all read this so they never disagree
double RingColorPlate::grownTriRadius(const QRectF& area) const {
    const double half = std::min(area.width(), area.height()) / 2;
    return half > 0 ? triInnerRadius(area) + 2 * _triSwell / half : RING_INNER_RATIO;
}

// ring mode: hue picks on the band, sat/value picks inside the triangle.
// In the hole a click goes to whichever element is closer — the band's
// inner edge or the triangle outline — so taps in the corners of the disk
// or just off the triangle's rim grab the ring instead of snapping to the
// triangle's nearest edge.
void RingColorPlate::beginPick(const QPointF& pos) {
    _dragRing = ringGrab(pos);
    setSwellTarget(_dragRing);
}

// hovering over a spot animates whichever element a click would hit —
// the band or the triangle — a pixel thicker; feedback for which element
// a click will hit, mirroring the slider thumb's grow-on-hover
void RingColorPlate::setSwellTarget(bool ring) {
    setTargets(ring, !ring);
}

void RingColorPlate::clearSwellTarget() {
    setTargets(false, false);
}

void RingColorPlate::setTargets(bool ring, bool tri) {
    if (ring == _ringTarget && tri == _triTarget) return;
    _ringTarget = ring;
    _triTarget = tri;
    if (!_anim_timer->isActive()) {
        _last_tick_ms = 0;
        _elapsed.start();
        _anim_timer->start();
    }
}

void RingColorPlate::onSwellTick() {
    const qint64 now = _elapsed.elapsed();
    const double dt = _last_tick_ms == 0 ? SWELL_TICK_MS / 1000.0
                                         : (now - _last_tick_ms) / 1000.0;
    _last_tick_ms = now;
    // grow fast toward the target, ease off when leaving — same asymmetric
    // feel as ColorSlider::onAnimationTick
    auto approach = [dt](double v, bool target) {
        return std::clamp(v + (target ? dt * 8.0 : -dt * 4.0), 0.0, 1.0);
    };
    const double ring = approach(_swell, _ringTarget);
    const double tri = approach(_triSwell, _triTarget);
    if (ring == _swell && tri == _triSwell) {
        _anim_timer->stop();
        _last_tick_ms = 0;
        return;
    }
    _swell = ring;
    _triSwell = tri;
    update();
}

Color RingColorPlate::colorAt(const QPointF& pos) const {
    const auto local = polarFromScreen(activeArea(), pos);
    auto c = color();
    if (_dragRing) {
        // pick a hue on the ring — angle only; sat/value keep their current values
        c.set(PLATE_HUE, polarAngle(local));
        return c;
    }
    // pick sat/value inside the triangle: v = apex + white barycentric weights,
    // s = apex / v (apex contributes s·v to the point, white v·(1−s))
    const auto vs = triangleVerts(grownTriRadius(activeArea()), c[PLATE_HUE]);
    const auto [wa, wb, ww] = barycentric(local, vs[0], vs[1], vs[2]);
    const double v = std::clamp(wa + ww, 0.0, 1.0);
    const double s = v > 0.0 ? std::clamp(wa / v, 0.0, 1.0) : 0.0;
    c.set(PLATE_SAT, s);
    c.set(PLATE_VAL, v);
    return c;
}

void RingColorPlate::colorChanged() {
    // hue marker sits mid-band on the ring
    const double band = (1.0 + RING_INNER_RATIO) / 2.0;
    _ringMarker = polarPoint(color()[PLATE_HUE], band);

    // any hue change must rotate the triangle — a coarse threshold
    // would quantize its rotation into visible steps and lag the marker
    if (channelDelta(plateSpace(), PLATE_HUE, _built_hue, color()[PLATE_HUE]) > 1e-4) {
        _triangle = {};
    }

    // sat/value marker inside the triangle: barycentric mix of the
    // apex (s·v), white (v·(1−s)) and black (1−v) corners
    // absurd channel values (OKLCH etc.) can come out NaN/Inf — sanitize
    // before clamping or the marker teleports to a corner
    const double hue = std::isfinite(color()[PLATE_HUE]) ? color()[PLATE_HUE] : 0.0;
    const auto vs = triangleVerts(triInnerRadius(activeArea()), hue);
    const double raw_v = color()[PLATE_VAL];
    const double raw_s = color()[PLATE_SAT];
    double v = std::isfinite(raw_v) ? std::clamp(raw_v, 0.0, 1.0) : 1.0;
    // Out-of-range or broken s (OKHSV conversion yields negatives at
    // extreme chroma) means "saturated beyond representable" — treat as
    // maximum and let the gamut projection find the boundary, not zero.
    double s = std::isfinite(raw_s) ? raw_s : 1.0;
    s = s < 0.0 ? 1.0 : std::clamp(s, 0.0, 1.0);
    // Project only genuinely out-of-range conversions. In-range (s,v) —
    // including points this widget just emitted along the v=1 edge —
    // must not be re-projected, or the marker flickers between boundary
    // positions while dragging.
    const bool outOfRange = !std::isfinite(raw_s) || !std::isfinite(raw_v) ||
                            raw_s < 0.0 || raw_s > 1.0 || raw_v < 0.0 || raw_v > 1.0;
    // Out-of-gamut spaces can produce an (s,v) pair whose RGB escapes the
    // gamut. Reduce saturation until the color lands inside sRGB — the
    // marker then sits on the gamut boundary, not in a corner.
    auto inGamut = [&](double sat, double val) {
        Color c = color();
        c.set(PLATE_HUE, hue); // use the sanitized hue, not the raw channel
        c.set(PLATE_SAT, sat);
        c.set(PLATE_VAL, val);
        auto rgb = c.converted(Space::Type::RGB);
        if (!rgb) return true; // can't verify — don't fight it
        for (int i = 0; i < 3; ++i) {
            // non-finite = broken conversion, not a gamut violation
            if (!std::isfinite((*rgb)[i])) return true;
            if ((*rgb)[i] < 0.0 || (*rgb)[i] > 1.0) return false;
        }
        return true;
    };
    if (outOfRange && !inGamut(s, v)) {
        double lo = 0.0, hi = s;
        for (int i = 0; i < 12 && hi - lo > 1e-3; ++i) {
            double mid = (lo + hi) / 2.0;
            (inGamut(mid, v) ? lo : hi) = mid;
        }
        s = lo;
        // degenerate case: still out at s=0 — walk value down to the
        // always-displayable gray edge
        if (!inGamut(s, v)) {
            double vlo = 0.0, vhi = v;
            for (int i = 0; i < 12 && vhi - vlo > 1e-3; ++i) {
                double mid = (vlo + vhi) / 2.0;
                (inGamut(s, mid) ? vlo : vhi) = mid;
            }
            v = vlo;
        }
    }
    _triMarker = vs[0] * (s * v) + vs[2] * (v * (1 - s)) + vs[1] * (1 - v);
}

std::vector<PlateMarker> RingColorPlate::markers() const {
    const auto active = activeArea();
    std::vector<PlateMarker> pts;
    if (_ringMarker) {
        QPointF p = *_ringMarker;
        const double half = std::min(active.width(), active.height()) / 2;
        const double r = std::hypot(p.x(), p.y());
        if (r > 0 && half > 0) p *= (r + RING_MARKER_OFFSET / half) / r;
        // the hue marker fills with the pure hue the band shows at its angle
        auto hue = color();
        hue.set(PLATE_SAT, 1);
        hue.set(PLATE_VAL, 1);
        pts.push_back({polarToScreen(active, p), QColor::fromRgb(QRgb(hue.toARGB()))});
    }
    if (_triMarker) {
        pts.push_back({polarToScreen(active, *_triMarker), QColor::fromRgb(QRgb(color().toARGB()))});
    }
    return pts;
}

// triangle vertices in widget coordinates — same transform as the plate image
std::array<QPointF, 3> RingColorPlate::triangleScreenVerts(const QRectF& area) const {
    const auto local = triangleVerts(grownTriRadius(area), color()[PLATE_HUE]);
    return {polarToScreen(area, local[0]), polarToScreen(area, local[1]), polarToScreen(area, local[2])};
}

void RingColorPlate::hideEvent(QHideEvent* event) {
    _dragRing = false;
    _ringTarget = _triTarget = false;
    _swell = _triSwell = 0.0;
    ColorPlate::hideEvent(event);
}

void RingColorPlate::mouseMoveEvent(QMouseEvent* event) {
    ColorPlate::mouseMoveEvent(event);
    if (!(event->buttons() & Qt::LeftButton)) {
        setSwellTarget(ringGrab(event->position()));
    }
}

void RingColorPlate::mouseReleaseEvent(QMouseEvent* event) {
    ColorPlate::mouseReleaseEvent(event);
    _dragRing = false;
    setSwellTarget(ringGrab(event->position()));
}

void RingColorPlate::leaveEvent(QEvent* event) {
    clearSwellTarget();
    ColorPlate::leaveEvent(event);
}

void RingColorPlate::paintPlate(QPainter& p) {
    constexpr int resolution = 256;
    const auto a = area();
    // the ring image is hue-independent: build it once
    // the builders produce sRGB bytes; convert once, at build time
    if (_ring.isNull()) {
        _ring = buildColorRing(resolution, color());
        _ring.setColorSpace(QColorSpace(QColorSpace::SRgb));
        toSurface(_ring);
    }
    // the pixel inset converts to a size-dependent local radius, so the
    // cached image must also rebuild when the plate's size changes
    const double tri_inner = triInnerRadius(a);
    if (_triangle.isNull() || std::abs(tri_inner - _built_inner) > 1e-4) {
        _built_hue = color()[PLATE_HUE];
        _built_inner = tri_inner;
        _triangle = buildColorTriangle(resolution, color(), tri_inner);
        _triangle.setColorSpace(QColorSpace(QColorSpace::SRgb));
        toSurface(_triangle);
    }

    const auto square = polarSquare(a);
    const double sz = square.width();

    // clip — the band normally sits a pixel inside both edges, reserving
    // room so it can swell a pixel on each side while the cursor is over a
    // spot that would grab it; the image carries a few pixels of margin
    // past both edges precisely so the clip has room to grow
    const double swell = _swell;
    QPainterPath clip;
    const double inner = sz * RING_INNER_RATIO + (1.0 - swell) * 2;
    clip.setFillRule(Qt::OddEvenFill); // carve out the hole
    clip.addEllipse(square.adjusted(1.0 - swell, 1.0 - swell, swell - 1.0, swell - 1.0));
    clip.addEllipse(QRectF(square.center().x() - inner / 2, square.center().y() - inner / 2, inner, inner));
    p.setClipPath(clip);

    // round the target rect outwards so the image edge never lands inside the
    // clip — toRect() can pull it up to a pixel in on one side, leaving an uncovered jagged strip
    p.drawImage(square.toAlignedRect(), _ring);

    // the triangle lives inside the ring's hole — its own clip
    const auto triVerts = triangleScreenVerts(a);
    QPainterPath triClip;
    triClip.addPolygon({triVerts[0], triVerts[1], triVerts[2]});
    triClip.closeSubpath();
    p.setClipPath(triClip);
    p.drawImage(square.toAlignedRect(), _triangle);

    p.setClipping(false);

    // border — tracks the band's outer edge so the outline always hugs it
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    drawStandardBorder(p, a.adjusted(1.0 - swell, 1.0 - swell, swell - 1.0, swell - 1.0),
                       dark, PLATE_CORNER_RADIUS, devicePixelRatio(), true);

    // inner border of the ring — shrink the rect concentrically and draw
    // outwards so the relief lands on the band, not inside the hole;
    // enlarge by one physical pixel per side (device-ratio aware)
    double inset = std::min(a.width(), a.height()) * (1.0 - RING_INNER_RATIO) / 2.0 - 1.0 / devicePixelRatio();
    inset -= 1.0 - swell;
    drawStandardBorder(p, a.adjusted(inset, inset, -inset, -inset), dark, PLATE_CORNER_RADIUS, devicePixelRatio(), true, false);
    // triangle border — same soft relief the standard border gives other
    // shapes: 1px concentric strokes marching inward with decaying alpha.
    // Insetting each edge of an equilateral triangle by d shrinks its
    // circumradius by 2d; the path is in physical pixels.
    const QPointF triCenter = (triVerts[0] + triVerts[1] + triVerts[2]) / 3.0;
    const double dpr = devicePixelRatio();
    drawBorderShape(p, a, getStandardBorderColor(dark), dpr, [&](QRectF&, int step) {
        QPainterPath path;
        QPolygonF poly;
        for (const auto& v : triVerts) {
            const QPointF d = (v - triCenter) * dpr;
            const double r = std::hypot(d.x(), d.y());
            poly << (r > 2.0 * step ? triCenter * dpr + d * ((r - 2.0 * step) / r) : triCenter * dpr);
        }
        path.addPolygon(poly);
        path.closeSubpath();
        return path;
    });
}

} // namespace Linea::UI
