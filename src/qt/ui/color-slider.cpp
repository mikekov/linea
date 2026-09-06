// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A slider with colored background.
 */

#include "color-slider.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QTimer>

#include "colors/color-set.h"
#include "colors/manager.h"
#include "colors/spaces/enum.h"
#include "colors/spaces/gamut.h"
#include "../util/drawing-utils.h"

using namespace Linea::UI;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr int    THUMB_SPACE      = 16;
static constexpr int    TRACK_HEIGHT     = 10;
static constexpr int    THUMB_SIZE       = TRACK_HEIGHT + 2;
static constexpr int    RING_THICKNESS   = 2;
static constexpr int    CHECKERBOARD_TILE = TRACK_HEIGHT / 2;
static constexpr int    ANIMATION_INTERVAL_MS = 16; // ~60 fps

// Error colors for empty color-set (ARGB)
static constexpr uint32_t ERR_DARK  = 0xFF00FF00u; // green  (ARGB)
static constexpr uint32_t ERR_LIGHT = 0xFFFF00FFu; // magenta

namespace Linea::UI {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Return a QRect describing the gradient track area within a widget of the
 * given full size, analogous to get_active_area() in the GTK version.
 */
static QRect activeArea(int fullWidth, int fullHeight) {
    int width = fullWidth - THUMB_SPACE;
    if (width <= 0) return {};
    int left = THUMB_SPACE / 2;
    return QRect(left, 0, width, fullHeight);
}

/**
 * Map an x-coordinate within the widget to a normalised [0, 1] value.
 */
double ColorSlider::valueAtX(double x) const {
    QRect area = activeArea(width(), height());
    if (area.isEmpty()) return 0.0;
    return std::clamp((x - area.left()) / area.width(), 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ColorSlider::ColorSlider(std::shared_ptr<Inkscape::Colors::ColorSet> colors,
                         Inkscape::Colors::Space::Component component,
                         QWidget* parent)
    : QWidget(parent)
    , _colors(std::move(colors))
    , _component(std::move(component))
{
    setObjectName("ColorSlider");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // Allow keyboard focus

    _ring_size      = THUMB_SIZE;
    _ring_thickness = RING_THICKNESS;

    _anim_timer = new QTimer(this);
    _anim_timer->setInterval(ANIMATION_INTERVAL_MS);
    connect(_anim_timer, &QTimer::timeout, this, &ColorSlider::onAnimationTick);

    _changed_connection = _colors->signal_changed.connect([this]() {
        update();
    });
}

ColorSlider::~ColorSlider() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

double ColorSlider::getScaled() const {
    if (_colors->isEmpty()) return 0.0;
    return _colors->getAverage(_component) * _component.scale;
}

void ColorSlider::setScaled(double value) {
    if (!_colors->isValid(_component)) return;
    _colors->setAll(_component, value / _component.scale);
}

// ---------------------------------------------------------------------------
// Size hint
// ---------------------------------------------------------------------------

QSize ColorSlider::sizeHint() const {
    return QSize(100, THUMB_SIZE + 4);
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void ColorSlider::updateComponent(double x) {
    if (_colors->isValid(_component) && _colors->setAll(_component, valueAtX(x))) {
        Q_EMIT valueChanged();
    }
}

void ColorSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() == ::Qt::LeftButton) {
        _dragging = true;
        updateComponent(event->position().x());
        event->accept();
        setFocus();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ColorSlider::mouseMoveEvent(QMouseEvent* event) {
    if (_dragging && (event->buttons() & ::Qt::LeftButton)) {
        updateComponent(event->position().x());
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void ColorSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == ::Qt::LeftButton) {
        _dragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ColorSlider::enterEvent(QEnterEvent* event) {
    _hover = true;
    startAnimation();
    QWidget::enterEvent(event);
}

void ColorSlider::leaveEvent(QEvent* event) {
    _hover = false;
    startAnimation();
    QWidget::leaveEvent(event);
}

void ColorSlider::keyPressEvent(QKeyEvent* event) {
    if (event->key() == ::Qt::Key_Left) {
        double current = getScaled();
        double step = _component.scale / 100.0; // 1% of range
        setScaled(std::max(0.0, current - step));
        event->accept();
    } else if (event->key() == ::Qt::Key_Right) {
        double current = getScaled();
        double step = _component.scale / 100.0; // 1% of range
        setScaled(std::min(_component.scale * 1.0, current + step));
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

// ---------------------------------------------------------------------------
// Thumb animation
// ---------------------------------------------------------------------------

void ColorSlider::startAnimation() {
    if (!_anim_timer->isActive()) {
        _last_tick_ms = 0;
        _elapsed.start();
        _anim_timer->start();
    }
}

void ColorSlider::onAnimationTick() {
    qint64 now = _elapsed.elapsed();
    double dt;
    if (_last_tick_ms == 0) {
        dt = ANIMATION_INTERVAL_MS / 1000.0;
    } else {
        dt = (now - _last_tick_ms) / 1000.0;
    }
    _last_tick_ms = now;

    // grow ring on hover, shrink it on leave (mirrors GTK tick callback)
    double change = dt * (_hover ? 12.0 : 6.0);

    double newSize      = std::clamp(_ring_size      + (_hover ? -change :  change),
                                     THUMB_SIZE - 1.0, static_cast<double>(THUMB_SIZE));
    double newThickness = std::clamp(_ring_thickness + (_hover ?  change : -change),
                                     static_cast<double>(RING_THICKNESS), RING_THICKNESS + 1.0);

    update();

    if (newSize == _ring_size && newThickness == _ring_thickness) {
        _anim_timer->stop();
        _last_tick_ms = 0;
    } else {
        _ring_size      = newSize;
        _ring_thickness = newThickness;
    }
}

// ---------------------------------------------------------------------------
// Painting helpers
// ---------------------------------------------------------------------------

/**
 * Fill @p buf (row of @p w pixels, ARGB32) with a 2×2 repeating checkerboard.
 * @p dark / @p light are ARGB uint32 values.
 */
static void fillCheckerRow(uint32_t* buf, int w, int y,
                            uint32_t dark, uint32_t light) {
    for (int x = 0; x < w; ++x) {
        bool darkCell = ((x / CHECKERBOARD_TILE) ^ (y / CHECKERBOARD_TILE)) & 1;
        buf[x] = darkCell ? dark : light;
    }
}

/**
 * Draw a slider thumb ring at the given location.
 */
static void drawSliderThumb(QPainter& p, const QPointF& location, double size, double thickness, const QColor& fill, const QColor& stroke) {
    double radius = size / 2.0;
    QPointF center = location;

    // outer stroke
    p.setPen(QPen(stroke, thickness + 2.0));
    p.setBrush(::Qt::NoBrush);
    p.drawEllipse(center, radius, radius);

    // inner ring
    p.setPen(QPen(fill, thickness));
    p.drawEllipse(center, radius, radius);
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------

void ColorSlider::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect area = activeArea(width(), height());
    if (area.isEmpty()) return;

    // --- Build the track rectangle (with rounded ends, centred vertically) ---
    QRect track = area;
    track.adjust(-4, 0, 4, 0); // stretch so the rounded ends sit under thumb centres
    int vMargin = (area.height() - TRACK_HEIGHT) / 2;
    track.adjust(0, vMargin, 0, -vMargin);
    track.setHeight(TRACK_HEIGHT);

    double radius = TRACK_HEIGHT / 2.0;
    QPainterPath trackPath;
    trackPath.addRoundedRect(track, radius, radius);

    p.save();
    p.setClipPath(trackPath);

    bool const isAlpha = (_component.id == "alpha");

    // --- Error state: empty color set ---
    if (_colors->isEmpty()) {
        // checkerboard with error colors (green / magenta)
        QImage errImg(track.width(), track.height(), QImage::Format_ARGB32);
        for (int y = 0; y < errImg.height(); ++y) {
            auto row = reinterpret_cast<uint32_t*>(errImg.scanLine(y));
            fillCheckerRow(row, errImg.width(), y, ERR_DARK, ERR_LIGHT);
        }
        p.drawImage(track, errImg);
        p.restore();
        return;
    }

    // --- Alpha channel: paint checkerboard background first ---
    if (isAlpha) {
        bool dark = Linea::UI::isDarkPalette(*this);
        uint32_t col1 = dark ? 0xFF666666u : 0xFFCCCCCCu;
        uint32_t col2 = dark ? 0xFF333333u : 0xFF999999u;

        QImage bgImg(track.width(), track.height(), QImage::Format_ARGB32);
        for (int y = 0; y < bgImg.height(); ++y) {
            auto row = reinterpret_cast<uint32_t*>(bgImg.scanLine(y));
            fillCheckerRow(row, bgImg.width(), y, col1, col2);
        }
        p.drawImage(track, bgImg);
    }

    // --- Gradient row ---
    int gradW = track.width();
    if (_gr_width != gradW) {
        _gr_buffer.resize(gradW);
        _gr_width = gradW;
    }

    auto paintColor = _colors->getAverage();
    if (!isAlpha) {
        paintColor.enableOpacity(false);
    }

    double lim = gradW > 1 ? gradW - 1.0 : 1.0;
    auto spaceRgb = Inkscape::Colors::Manager::get().find(Inkscape::Colors::Space::Type::RGB);
    for (int x = 0; x < gradW; ++x) {
        paintColor.set(_component.index, x / lim);
        auto c = Inkscape::Colors::to_gamut_css(paintColor, spaceRgb);
        // to_gamut_css returns ABGR; convert to ARGB for Qt
        uint32_t abgr = c.toABGR();
        uint8_t a = (abgr >> 24) & 0xFF;
        uint8_t b = (abgr >> 16) & 0xFF;
        uint8_t g = (abgr >>  8) & 0xFF;
        uint8_t r = (abgr      ) & 0xFF;
        _gr_buffer[x] = (static_cast<uint32_t>(a) << 24)
                      | (static_cast<uint32_t>(r) << 16)
                      | (static_cast<uint32_t>(g) <<  8)
                      | static_cast<uint32_t>(b);
    }

    QImage gradImg(reinterpret_cast<uchar*>(_gr_buffer.data()),
                   gradW, 1, QImage::Format_ARGB32);
    // Scale the single-row image to the full track height
    p.drawImage(track, gradImg.scaled(gradW, track.height(), ::Qt::IgnoreAspectRatio,
                                      ::Qt::FastTransformation));
    p.restore(); // end clip

    // --- Track border (using standard border function) ---
    bool dark = Linea::UI::isDarkPalette(*this);
    int deviceScale = devicePixelRatio();
    Linea::UI::drawStandardBorder(p, QRectF(track), dark, radius, deviceScale, false, true);

    // --- Thumb ---
    if (!_colors->isValid(_component)) return;

    double value = std::clamp(_colors->getAverage(_component), 0.0, 1.0);
    if (!std::isfinite(value)) return;

    double thumbX = area.left() + value * area.width();
    double thumbY = area.top() + area.height() / 2.0;

    // Get theme background color
    QColor ring = palette().color(QPalette::Window);

    // Calculate stroke color based on luminance
    bool ringDark = getLuminance(ring) < 0.5;
    float x = ringDark ? 1.0f : 0.0f;
    float alpha = ringDark ? 0.40f : 0.25f;
    QColor stroke(static_cast<int>(x * 255), static_cast<int>(x * 255),
                  static_cast<int>(x * 255), static_cast<int>(alpha * 255));

    drawSliderThumb(p, QPointF(thumbX, thumbY), _ring_size, _ring_thickness, ring, stroke);
}

} // namespace Linea::UI
