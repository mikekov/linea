// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPlate — color plate widget: base class, picking, overlays, factory.
 */

#include "color-plate.h"

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <chrono>
#include <cmath>

#include "color-plate-shapes.h"
#include "colors/spaces/base.h"
#include "colors/spaces/components.h"
#include "qt/util/drawing-utils.h"

namespace Linea::UI {

using namespace Inkscape::Colors;
using namespace Inkscape::Colors::Space;

constexpr double RING_SIZE = 8;
constexpr double RING_THICKNESS = 1.5;
// presses this far outside the plate still pick the nearest edge color —
// the tolerance the widget padding used to provide, now covering popup
// margins and layout spacing around the plate
constexpr double PICK_MARGIN = 3;

constexpr bool TEST_TIMING = false;

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

QColorSpace gamutColorSpace(Gamut gamut) {
    switch (gamut) {
        case Gamut::DisplayP3:
            return QColorSpace(QColorSpace::DisplayP3);
        case Gamut::Rec2020:
            return QColorSpace(QColorSpace::Bt2020);
        default:
            return QColorSpace(QColorSpace::SRgb);
    }
}

void ColorPlate::toSurface(QImage& img) const {
    const auto target = gamutColorSpace(_surface);
    if (img.colorSpace() != target) {
        img.convertToColorSpace(target, QImage::Format_ARGB32_Premultiplied);
    }
}

ColorPlate::ColorPlate(Type source, Type space, int fixed_channel, Gamut surface, QWidget* parent)
    : QWidget(parent)
    , _source(source)
    , _space(space)
    , _fixed_channel(fixed_channel)
    , _surface(surface)
    , _color(space, {0, 0, 0}) {
    _color.setOpacity(1);
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    qApp->installEventFilter(this);
}

ColorPlate::~ColorPlate() {
    qApp->removeEventFilter(this);
    for (auto& overlay : _overlays) {
        delete overlay;
    }
}

// ---------------------------------------------------------------------------
// factory
// ---------------------------------------------------------------------------

ColorPlate* ColorPlate::create(Type source, ColorPlateType shape, const PlateGamut& gamut, QWidget* parent) {
    const auto& spec = Components::get(source).plate();
    ColorPlate* plate = nullptr;
    switch (shape) {
        case ColorPlateType::Wheel:
            if (spec.polar != Type::NONE) plate = new WheelColorPlate(source, spec.polar, gamut, parent);
            break;
        case ColorPlateType::Ring:
            if (spec.polar != Type::NONE) plate = new RingColorPlate(source, spec.polar, gamut, parent);
            break;
        case ColorPlateType::Rect:
            if (spec.rect != Type::NONE) plate = new RectColorPlate(source, spec, gamut, parent);
            break;
        default:
            break;
    }

    // Speed test — use this to evaluate how quickly we can rebuild a color wheel
    if constexpr (TEST_TIMING) {
        if (plate) {
            QImage offscreen(1024, 1024, QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&offscreen);

            auto old_time = std::chrono::high_resolution_clock::now();
            Color color(Type::OKHSL, {0.5, 0.5, 0.5});
            for (int i = 0; i < 100; ++i) {
                color.set(0, i / 100.0);
                plate->setColor(color);
                plate->render(&painter);
            }
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - old_time);
            qDebug("render time for test wheel: %d ms", static_cast<int>(elapsed.count()));
        }
    }

    return plate;
}

bool ColorPlate::supports(Type source) {
    return Components::get(source).plate().polar != Type::NONE;
}

// ---------------------------------------------------------------------------
// color in/out
// ---------------------------------------------------------------------------

std::optional<unsigned> ColorPlate::hueChannel() const {
    return Components::get(_space).hue_index();
}

bool ColorPlate::achromatic(const Color& plate) const {
    const auto& comps = Components::get(_space);
    const auto hue = hueChannel();
    for (unsigned i = 0; i < comps.size(); ++i) {
        if (hue && i == *hue) continue;
        if (plate[i] <= ACHROMATIC_CHROMA) return true;
    }
    return false;
}

bool ColorPlate::sharesHue(Type type) const {
    const auto& spec = Components::get(type).plate();
    return spec.shared_hue && (_space == spec.polar || _space == spec.rect);
}

void ColorPlate::setColor(const Color& color) {
    if (_drag) return;

    auto copy = color.converted(_space);
    auto dest = copy.value_or(Color{_space, {0, 0, 0}});

    // When the source's hue convention matches the plate's, the stored
    // hue is authoritative: the converted hue carries residue that jumps
    // arbitrarily for grays and flips the 0/360 representation at the
    // seam. Otherwise keep the last hue shown for achromatic colors,
    // whose converted hue is meaningless.
    const auto type = color.getSpace() ? color.getSpace()->getType() : Type::NONE;
    const auto hue_ch = hueChannel();
    if (hue_ch && sharesHue(type)) {
        if (const auto src_hue = Components::get(type).hue_index()) {
            dest.set(*hue_ch, color[*src_hue]);
        }
    } else if (hue_ch && achromatic(dest) && _last_hue >= 0.0) {
        dest.set(*hue_ch, _last_hue);
    }
    if (hue_ch) {
        _last_hue = dest[*hue_ch];
    }
    dest.setOpacity(1);

    _color = std::move(dest);
    colorChanged();
    update();
    syncOverlays();
}

sigc::connection ColorPlate::connectColorChanged(sigc::slot<void(const Color&)> cb) {
    return _signal_color_changed.connect(std::move(cb));
}

void ColorPlate::emitColor() {
    auto color = _color.converted(_source);
    if (color) {
        // When the plate holds hue fixed (rectangular plate), that
        // hue is authoritative: the conversion residue makes the hue
        // field wobble or flip across the 0/360 seam. Same for
        // achromatic picks, where the converted hue is meaningless.
        const auto hue_ch = hueChannel();
        if (hue_ch && (static_cast<unsigned>(_fixed_channel) == *hue_ch || achromatic(_color)) &&
            sharesHue(_source)) {
            if (const auto src_hue = Components::get(_source).hue_index()) {
                color->set(*src_hue, _color[*hue_ch]);
            }
        }
        _signal_color_changed.emit(*color);
    } else {
        qWarning("Color conversion from type %d to type %d failed.", int(_space), int(_source));
    }
}

// ---------------------------------------------------------------------------
// geometry helpers
// ---------------------------------------------------------------------------

QRectF ColorPlate::area() const {
    return QRectF(0, 0, width(), height());
}

QRectF ColorPlate::activeArea() const {
    return area().adjusted(1, 1, -1, -1);
}

double ColorPlate::channelDelta(Type space, int channel, double a, double b) {
    double diff = std::fabs(a - b);
    const auto hue = Components::get(space).hue_index();
    if (hue && *hue == static_cast<unsigned>(channel)) {
        diff = std::min(diff, 1.0 - diff); // hue wraps around
    }
    return diff;
}

// ---------------------------------------------------------------------------
// picking
// ---------------------------------------------------------------------------

void ColorPlate::pickAt(QPointF pos) {
    const auto active = activeArea();
    if (active.width() <= 0 || active.height() <= 0) return;

    _color = colorAt(pos);
    colorChanged();
    update();
    syncOverlays();
    emitColor();
}

// ---------------------------------------------------------------------------
// indicator overlays
// ---------------------------------------------------------------------------

void ColorPlate::syncOverlays() {
    const auto pts = markers();
    while (_overlays.size() > pts.size()) {
        delete _overlays.back();
        _overlays.pop_back();
    }
    _overlays.resize(pts.size());

    auto host = window();
    const auto active = activeArea();
    const bool show = isVisible() && host && active.width() > 0 && active.height() > 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        auto& overlay = _overlays[i];
        if (!show) {
            if (overlay) {
                overlay->hide();
            }
            continue;
        }
        if (!overlay || overlay->parentWidget() != host) {
            delete overlay;
            overlay = new IndicatorOverlay(host);
        }
        const QPointF center = mapTo(host, pts[i]);
        overlay->placeAt(center);
        overlay->show();
        overlay->raise();
    }
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------

void ColorPlate::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (area().width() <= 0 || area().height() <= 0) return;

    paintPlate(p);
    syncOverlays();
}

void ColorPlate::hideEvent(QHideEvent* event) {
    _drag = false;
    for (auto& overlay : _overlays) {
        if (overlay) {
            overlay->hide();
        }
    }
    QWidget::hideEvent(event);
}

void ColorPlate::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    syncOverlays();
}

void ColorPlate::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncOverlays();
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
            beginPick(pos);
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
    beginPick(e->position());
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
