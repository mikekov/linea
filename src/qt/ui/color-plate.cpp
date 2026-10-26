// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPlate — color plate widget.
 */

#include "color-plate.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <array>
#include <cmath>

#include "colors/color.h"
#include "colors/spaces/base.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

constexpr double RING_SIZE = 8;
constexpr double RING_THICKNESS = 1.5;
// inner radius of a ring plate, as a fraction of its outer radius
constexpr double RING_INNER_RATIO = 0.80;
// presses this far outside the plate still pick the nearest edge color —
// the tolerance the widget padding used to provide, now covering popup
// margins and layout spacing around the plate
constexpr double PICK_MARGIN = 3;

// The color indicator is drawn by a dedicated widget parented to the
// top-level window rather than by the plate itself: a widget cannot paint
// outside its own bounds, but the indicator may legitimately hang over the
// plate's edge — e.g. into a surrounding popup's margins.
class IndicatorOverlay : public QWidget {
public:
    explicit IndicatorOverlay(QWidget* parent)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        const int side = static_cast<int>(std::ceil(RING_SIZE + RING_THICKNESS + 6));
        setFixedSize(side, side);
    }

    void placeAt(const QPointF& hostCenter) {
        const int half = width() / 2;
        const QPoint topLeft(qRound(hostCenter.x()) - half, qRound(hostCenter.y()) - half);
        const QPointF center = hostCenter - QPointF(topLeft);
        if (pos() != topLeft) {
            move(topLeft);
        }
        if (_center != center) {
            _center = center;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        drawThumbRing(p, _center, RING_SIZE, RING_THICKNESS, *this);
    }

private:
    QPointF _center;
};

// ---------------------------------------------------------------------------
// helper functions
// ---------------------------------------------------------------------------

namespace {

void setColorHelper(Color& color, int channel1, int channel2, double x, double y, bool disc) {
    if (disc) {
        double dist = std::hypot(x, y);
        double angle = (std::atan2(x, y) + M_PI) / (2 * M_PI);
        color.set(channel1, angle);
        color.set(channel2, dist);
    } else {
        color.set(channel1, x);
        color.set(channel2, 1.0 - y);
    }
}

QPointF getColorCoordinates(double val1, double val2, bool circular) {
    val1 = std::clamp(val1, 0.0, 1.0);
    val2 = std::clamp(val2, 0.0, 1.0);
    if (circular) {
        double angle = val1 * 2 * M_PI - M_PI;
        return {std::sin(angle) * val2, std::cos(angle) * val2};
    }
    return {val1, 1.0 - val2};
}

// Build a rectangular plate image
QImage buildColorPlate(int resolution, const Color& base, int ch1, int ch2) {
    int size = resolution + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    auto color = base;
    color.addOpacity();
    for (int iy = 0; iy <= resolution; ++iy) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(iy));
        double y = iy / double(resolution);
        color.set(ch2, 1.0 - y);
        for (int ix = 0; ix <= resolution; ++ix) {
            color.set(ch1, ix / double(resolution));
            uint32_t argb = color.toARGB();
            line[ix] = QRgb(argb);
        }
    }
    return img;
}

// Build a circular (disc) wheel image
QImage buildColorWheel(int resolution, const Color& base, int ch1, int ch2) {
    int radius = resolution / 2;
    int size = radius * 2 + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    double limit = radius;
    double rsqr = std::pow(1.0 + 1.0 / radius, 2);
    Color color = base;
    for (int iy = -radius; iy <= radius; ++iy) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(iy + radius));
        double y = iy / limit;
        for (int ix = -radius; ix <= radius; ++ix) {
            double x = ix / limit;
            if (x * x + y * y > rsqr) continue;
            setColorHelper(color, ch1, ch2, x, y, true);
            line[ix + radius] = QRgb(color.toARGB());
        }
    }
    return img;
}

// Build a ring (annulus) wheel image — channel 1 varies with the angle,
// channel 2 is pinned to maximum (a pure hue ring)
QImage buildColorRing(int resolution, const Color& base, int ch1, int ch2, int chFixed) {
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
    color.set(ch2, 1.0);
    if (chFixed >= 0) color.set(chFixed, 1.0); // ring is the hue rainbow at max value
    for (int iy = -radius; iy <= radius; ++iy) {
        auto line = reinterpret_cast<QRgb*>(img.scanLine(iy + radius));
        double y = iy / limit;
        for (int ix = -radius; ix <= radius; ++ix) {
            double x = ix / limit;
            double rr = x * x + y * y;
            if (rr > outer_sqr || rr < inner_sqr) continue;
            double angle = (std::atan2(x, y) + M_PI) / (2 * M_PI);
            color.set(ch1, angle);
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
    const double a = hue * 2 * M_PI - M_PI; // same convention as getColorCoordinates
    for (int i = 0; i < 3; ++i) {
        double ang = a + i * 2.0 * M_PI / 3.0;
        vs[i] = QPointF(std::sin(ang), std::cos(ang)) * inner;
    }
    return vs;
}

// barycentric weights of p within triangle (a, b, c); any negative = outside
std::array<double, 3> barycentric(const QPointF& p, const QPointF& a, const QPointF& b, const QPointF& c) {
    double den = (b.y() - c.y()) * (a.x() - c.x()) + (c.x() - b.x()) * (a.y() - c.y());
    double wa = ((b.y() - c.y()) * (p.x() - c.x()) + (c.x() - b.x()) * (p.y() - c.y())) / den;
    double wb = ((c.y() - a.y()) * (p.x() - c.x()) + (a.x() - c.x()) * (p.y() - c.y())) / den;
    return {wa, wb, 1.0 - wa - wb};
}

// Build the ring's inner triangle image. Corners are lerped in sRGB between
// the hue, black and white vertices — matching the GTK wheel's approach of
// interpolating corner colors instead of per-pixel channel conversion.
QImage buildColorTriangle(int resolution, const Color& base, int chHue, int chSat, int chVal) {
    int radius = resolution / 2;
    int size = radius * 2 + 1;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const auto [apex, black, white] = triangleVerts(RING_INNER_RATIO, base[chHue]);
    Color corner = base;
    corner.set(chSat, 1.0);
    if (chVal >= 0) corner.set(chVal, 1.0);
    const QRgb cApex = QRgb(corner.toARGB());
    const double rA = qRed(cApex), gA = qGreen(cApex), bA = qBlue(cApex);

    // cover the triangle bounds, plus a couple of pixels of margin so the
    // antialiased clip edge has room to work
    const double eps = 2.0 / radius;
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

ColorPlate::ColorPlate(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    qApp->installEventFilter(this);
}

ColorPlate::~ColorPlate() {
    qApp->removeEventFilter(this);
    delete _indicatorOverlay;
    delete _triOverlay;
}

void ColorPlate::setBaseColor(Color color, int fixedChannel, int varChannel1, int varChannel2) {
    color.setOpacity(1);

    const auto type = color.getSpace() ? color.getSpace()->getType() : Space::Type::NONE;
    // in ring mode the triangle rebuild tracks hue — the first var channel —
    // not the fixed channel
    const int watched = _ring ? varChannel1 : fixedChannel;
    const bool watched_is_hue = watched == 0 && (type == Space::Type::HSV || type == Space::Type::OKHSV);

    double fixed_diff = std::fabs(_fixedChannelVal - color[watched]);
    if (watched_is_hue && _fixedChannelVal >= 0.0) {
        fixed_diff = std::min(fixed_diff, 1.0 - fixed_diff); // hue wraps around
    }

    const bool spaceChanged = _baseColor.getSpace() != color.getSpace();
    // ring mode: any hue change must rotate the triangle — a coarse threshold
    // would quantize its rotation into visible steps and lag the marker
    const double threshold = _ring ? 1e-4 : 0.005;
    bool needsRebuild = spaceChanged || fixed_diff > threshold || _channel1 != varChannel1 ||
                        _channel2 != varChannel2;
    _baseColor = std::move(color);
    if (needsRebuild) {
        _fixedChannelVal = _baseColor[watched];
        _fixedChannel = fixedChannel;
        _channel1 = varChannel1;
        _channel2 = varChannel2;
        if (_ring) {
            if (spaceChanged) _plate = {}; // the ring image itself is hue-independent
            _triangle = {};
        } else {
            _plate = {};
        }
        update();
    }
}

void ColorPlate::setDisc(bool disc) {
    if (_disc == disc) return;
    _disc = disc;
    _plate = {};
    update();
}

void ColorPlate::setRing(bool ring) {
    if (_ring == ring) return;
    _ring = ring;
    _plate = {};
    update();
}

void ColorPlate::setColor(const Color& color) {
    if (_drag) return;
    // Base implementation: convert to current plate space, update indicator only.
    // FastColorPlate in the factory overrides this with the proper fixed/var channel logic.
    auto plateType = _baseColor.getSpace() ? _baseColor.getSpace()->getType() : Space::Type::RGB;
    auto copy = color.converted(plateType);
    if (copy) {
        moveIndicatorTo(*copy);
    }
}

sigc::connection ColorPlate::connectColorChanged(sigc::slot<void(const Color&)> cb) {
    return _signal_color_changed.connect(std::move(cb));
}

void ColorPlate::moveIndicatorTo(const Color& color) {
    if (_ring) {
        // hue marker sits mid-band on the ring
        const double band = (1.0 + RING_INNER_RATIO) / 2.0;
        const QPointF ringPt = getColorCoordinates(color[_channel1], band, true);
        // sat/value marker inside the triangle: barycentric mix of the
        // apex (s·v), white (v·(1−s)) and black (1−v) corners
        // absurd channel values (OKLCH etc.) can come out NaN/Inf — sanitize
        // before clamping or the marker teleports to a corner
        const double hue = std::isfinite(color[_channel1]) ? color[_channel1] : 0.0;
        const auto vs = triangleVerts(RING_INNER_RATIO, hue);
        const double raw_v = _fixedChannel >= 0 ? color[_fixedChannel] : 1.0;
        const double raw_s = color[_channel2];
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
            Color c = color;
            c.set(_channel1, hue); // use the sanitized hue, not the raw channel
            c.set(_channel2, sat);
            if (_fixedChannel >= 0) c.set(_fixedChannel, val);
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
        const QPointF triPt = vs[0] * (s * v) + vs[2] * (v * (1 - s)) + vs[1] * (1 - v);
        if (_indicator == ringPt && _triIndicator == triPt) return;
        _indicator = ringPt;
        _triIndicator = triPt;
        syncIndicatorOverlay();
        return;
    }
    auto pt = getColorCoordinates(color[_channel1], color[_channel2], _disc);
    if (_indicator && *_indicator == pt) return;
    _indicator = pt;
    syncIndicatorOverlay();
}

// ---------------------------------------------------------------------------
// geometry helpers
// ---------------------------------------------------------------------------

QRectF ColorPlate::getArea() const {
    QRectF r(0, 0, width(), height());
    return r.adjusted(_padding, _padding, -_padding, -_padding);
}

QRectF ColorPlate::getActiveArea() const {
    return getArea().adjusted(1, 1, -1, -1);
}

// maps a position to plate coordinates, clamping onto the nearest pickable
// point — clicks past the colored area pick the nearest edge color
QPointF ColorPlate::screenToLocal(const QRectF& active, QPointF pt) const {
    if (_disc) {
        double nx = (pt.x() - active.left()) / active.width();
        double ny = (pt.y() - active.top()) / active.height();
        double min = std::min(active.width(), active.height());
        double sx = min / active.width();
        double sy = min / active.height();
        double cx = (nx * 2 - 1) / sx;
        double cy = (ny * 2 - 1) / sy;
        double dist = std::hypot(cx, cy);
        // ring mode needs interior coords for the triangle — only clamp
        // points past the outer edge
        if (dist > 1.0) {
            cx /= dist;
            cy /= dist;
        }
        return {cx, cy};
    }
    // clamp to active area
    double x = std::clamp(pt.x(), active.left(), active.right());
    double y = std::clamp(pt.y(), active.top(), active.bottom());
    // normalize to 0..1
    double nx = (x - active.left()) / active.width();
    double ny = (y - active.top()) / active.height();
    return {nx, ny};
}

QPointF ColorPlate::localToScreen(const QRectF& active, QPointF local) const {
    if (_disc) {
        double min = std::min(active.width(), active.height());
        double sx = min / active.width();
        double sy = min / active.height();
        double nx = (local.x() * sx + 1) / 2;
        double ny = (local.y() * sy + 1) / 2;
        return {active.left() + nx * active.width(), active.top() + ny * active.height()};
    }
    return {active.left() + local.x() * active.width(), active.top() + local.y() * active.height()};
}

Color ColorPlate::getColorAt(const QPointF& local) const {
    auto color = _baseColor;
    setColorHelper(color, _channel1, _channel2, local.x(), local.y(), _disc);
    return color;
}

void ColorPlate::syncOverlay(QPointer<IndicatorOverlay>& overlay, const std::optional<QPointF>& indicator) {
    auto host = window();
    const bool show = indicator.has_value() && isVisible() && host;
    if (!show) {
        if (overlay) {
            overlay->hide();
        }
        return;
    }
    if (!overlay || overlay->parentWidget() != host) {
        delete overlay;
        overlay = new IndicatorOverlay(host);
    }
    const QPointF center = mapTo(host, localToScreen(getActiveArea(), *indicator));
    overlay->placeAt(center);
    overlay->show();
    overlay->raise();
}

void ColorPlate::syncIndicatorOverlay() {
    syncOverlay(_indicatorOverlay, _indicator);
    syncOverlay(_triOverlay, _triIndicator);
}

void ColorPlate::fireColorChanged() {
    if (!_indicator) return;
    auto color = getColorAt(*_indicator);
    _signal_color_changed.emit(color);
}

void ColorPlate::select(const QPointF& local) {
    _indicator = local;
    syncIndicatorOverlay();
    fireColorChanged();
}

void ColorPlate::pickAt(QPointF pos) {
    auto local = screenToLocal(getActiveArea(), pos);
    if (_ring) {
        if (_dragRing) {
            selectRingPoint(local);
        } else {
            selectTrianglePoint(local);
        }
        return;
    }
    select(local);
}

// pick a hue on the ring — angle only; sat/value keep their current values
void ColorPlate::selectRingPoint(const QPointF& local) {
    double angle = (std::atan2(local.x(), local.y()) + M_PI) / (2 * M_PI);
    auto color = _baseColor;
    color.set(_channel1, angle);
    // dragging suppresses incoming setColor, so the hue-driven triangle
    // rebuild and both indicators must be updated locally
    setBaseColor(color, _fixedChannel, _channel1, _channel2);
    moveIndicatorTo(color);
    _signal_color_changed.emit(color);
}

// pick sat/value inside the triangle: v = apex + white barycentric weights,
// s = apex / v (apex contributes s·v to the point, white v·(1−s))
void ColorPlate::selectTrianglePoint(const QPointF& local) {
    const auto vs = triangleVerts(RING_INNER_RATIO, _baseColor[_channel1]);
    const auto [wa, wb, ww] = barycentric(local, vs[0], vs[1], vs[2]);
    const double v = std::clamp(wa + ww, 0.0, 1.0);
    const double s = v > 0.0 ? std::clamp(wa / v, 0.0, 1.0) : 0.0;
    auto color = _baseColor;
    color.set(_channel2, s);
    if (_fixedChannel >= 0) color.set(_fixedChannel, v);
    setBaseColor(color, _fixedChannel, _channel1, _channel2);
    moveIndicatorTo(color);
    _signal_color_changed.emit(color);
}

// ---------------------------------------------------------------------------
// plate rendering
// ---------------------------------------------------------------------------

void ColorPlate::rebuildPlate() {
    constexpr int resolution = 256;
    if (_ring) {
        _plate = buildColorRing(resolution, _baseColor, _channel1, _channel2, _fixedChannel);
    } else if (_disc) {
        _plate = buildColorWheel(resolution, _baseColor, _channel1, _channel2);
    } else {
        _plate = buildColorPlate(resolution, _baseColor, _channel1, _channel2);
    }
}

void ColorPlate::rebuildTriangle() {
    constexpr int resolution = 256;
    _triangle = buildColorTriangle(resolution, _baseColor, _channel1, _channel2, _fixedChannel);
}

// triangle vertices in widget coordinates — same transform as the plate image
std::array<QPointF, 3> ColorPlate::triangleScreenVerts(const QRectF& area) const {
    const auto local = triangleVerts(RING_INNER_RATIO, _baseColor[_channel1]);
    return {localToScreen(area, local[0]), localToScreen(area, local[1]), localToScreen(area, local[2])};
}

void ColorPlate::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF area = getArea();
    if (area.width() <= 0 || area.height() <= 0) return;

    if (_plate.isNull()) rebuildPlate();
    if (_ring && _triangle.isNull()) rebuildTriangle();

    // clip
    QPainterPath clip;
    std::array<QPointF, 3> triVerts;
    if (_ring) {
        double sz = std::min(area.width(), area.height());
        double inner = sz * RING_INNER_RATIO;
        clip.setFillRule(Qt::OddEvenFill); // carve out the hole
        clip.addEllipse(QRectF(area.center().x() - sz / 2, area.center().y() - sz / 2, sz, sz));
        clip.addEllipse(QRectF(area.center().x() - inner / 2, area.center().y() - inner / 2, inner, inner));
    } else if (_disc) {
        double sz = std::min(area.width(), area.height());
        QRectF circle(area.center().x() - sz / 2, area.center().y() - sz / 2, sz, sz);
        clip.addEllipse(circle);
    } else {
        clip.addRoundedRect(area, _radius, _radius);
    }
    p.setClipPath(clip);

    // draw the plate image scaled into its area — disc/ring always maps to a
    // square centered inside the rect, never stretched into an oval
    QRectF imgRect = area;
    if (_disc) {
        double sz = std::min(area.width(), area.height());
        imgRect = QRectF(area.center().x() - sz / 2, area.center().y() - sz / 2, sz, sz);
    }
    p.drawImage(imgRect.toRect(), _plate);

    // the triangle lives inside the ring's hole — its own clip
    if (_ring) {
        triVerts = triangleScreenVerts(area);
        QPainterPath triClip;
        triClip.addPolygon({triVerts[0], triVerts[1], triVerts[2]});
        triClip.closeSubpath();
        p.setClipPath(triClip);
        p.drawImage(imgRect.toRect(), _triangle);
    }

    p.setClipping(false);

    // border
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    drawStandardBorder(p, area, dark, _radius, devicePixelRatio(), _disc);
    if (_ring) {
        // inner border of the ring — shrink the rect concentrically and draw
        // outwards so the relief lands on the band, not inside the hole;
        // enlarge by one physical pixel per side (device-ratio aware)
        double inset = std::min(area.width(), area.height()) * (1.0 - RING_INNER_RATIO) / 2.0 - 1.0 / devicePixelRatio();
        drawStandardBorder(p, area.adjusted(inset, inset, -inset, -inset), dark, _radius, devicePixelRatio(), _disc, false);
        // triangle border
        p.setPen(QPen(getStandardBorderColor(dark), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon({triVerts[0], triVerts[1], triVerts[2]});
    }

    syncIndicatorOverlay();
}

void ColorPlate::hideEvent(QHideEvent* event) {
    _drag = false;
    _dragRing = false;
    if (_indicatorOverlay) {
        _indicatorOverlay->hide();
    }
    if (_triOverlay) {
        _triOverlay->hide();
    }
    QWidget::hideEvent(event);
}

void ColorPlate::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    syncIndicatorOverlay();
}

void ColorPlate::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncIndicatorOverlay();
}

// ---------------------------------------------------------------------------
// mouse events
// ---------------------------------------------------------------------------

// left presses in the dead space around the plate (popup margins, layout
// spacing) still pick the nearest edge color — like the old padding did.
// Those events are delivered to the plate's ancestors, so catch them here.
bool ColorPlate::eventFilter(QObject* watched, QEvent* event) {
    if (watched == this || !isVisible() || !watched->isWidgetType()) {
        return QWidget::eventFilter(watched, event);
    }
    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto me = static_cast<QMouseEvent*>(event);
            auto w = static_cast<QWidget*>(watched);
            if (me->button() != Qt::LeftButton || _drag || !w->isAncestorOf(this)) break;
            const QPointF pos = mapFromGlobal(me->globalPosition());
            if (!QRectF(rect()).adjusted(-PICK_MARGIN, -PICK_MARGIN, PICK_MARGIN, PICK_MARGIN).contains(pos)) break;
            _drag = true;
            if (_ring) {
                const auto local = screenToLocal(getActiveArea(), pos);
                _dragRing = std::hypot(local.x(), local.y()) >= RING_INNER_RATIO;
            }
            pickAt(pos);
            return true;
        }
        case QEvent::MouseMove:
            if (_drag) {
                pickAt(mapFromGlobal(static_cast<QMouseEvent*>(event)->globalPosition()));
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (_drag && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                _drag = false;
                return true;
            }
            break;
        default:
            break;
    }
    return QWidget::eventFilter(watched, event);
}

void ColorPlate::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);

    _drag = true;
    if (_ring) {
        const auto local = screenToLocal(getActiveArea(), e->position());
        _dragRing = std::hypot(local.x(), local.y()) >= RING_INNER_RATIO;
    }
    pickAt(e->position());
    e->accept();
}

void ColorPlate::mouseMoveEvent(QMouseEvent* e) {
    if (!_drag || !(e->buttons() & Qt::LeftButton)) return QWidget::mouseMoveEvent(e);

    pickAt(e->position());
    e->accept();
}

void ColorPlate::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return QWidget::mouseReleaseEvent(e);

    _drag = false;
    e->accept();
}

} // namespace Linea::UI
