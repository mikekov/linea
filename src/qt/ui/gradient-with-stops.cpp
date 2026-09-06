// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Gradient image widget with stop handles
 */
/*
 * Author:
 *   Michael Kowalski
 *
 * Copyright (C) 2020-2026 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "gradient-with-stops.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QCursor>
#include <QApplication>
#include <QStyle>
#include <algorithm>

#include "io/resource.h"
#include "object/sp-gradient.h"
#include "object/sp-stop.h"
#include "qt/util/drawing-utils.h"
#include "ui/cursor-utils.h"
#include "ui/util.h"
#include "util/drawing-utils.h"
#include "util/object-renderer.h"

namespace Linea::UI {

using std::round;
using namespace Inkscape::IO;

std::string getStopTemplatePath(const char* filename) {
    // "stop handle" template files path
    return Resource::get_filename(Resource::UIS, filename);
}

GradientWithStops::GradientWithStops(QWidget* parent)
    : QWidget(parent)
    , _template(getStopTemplatePath("gradient-stop.svg").c_str())
    , _tipTemplate(getStopTemplatePath("gradient-tip.svg").c_str())
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Set theme-based colors
    auto* style = qApp->style();
    _backgroundColor = style->standardPalette().color(QPalette::Base);
    _foregroundColor = style->standardPalette().color(QPalette::Text);
}

GradientWithStops::~GradientWithStops() = default;

void GradientWithStops::setGradient(SPGradient* gradient) {
    _gradient = gradient;

    // listen to release & changes
    _release  = gradient ? gradient->connectRelease([this](SPObject*){ setGradient(nullptr); }) : sigc::connection();
    _modified = gradient ? gradient->connectModified([this](SPObject*, guint){ modified(); }) : sigc::connection();

    modified();
    setEnabled(gradient != nullptr);
}

void GradientWithStops::modified() {
    // gradient has been modified

    // read all stops
    _stops.clear();

    if (_gradient) {
        SPStop* stop = _gradient->getFirstStop();
        while (stop) {
            _stops.push_back(Stop {
                .offset = stop->offset, .color = stop->getColor(), .opacity = stop->getColor().getOpacity()
            });
            stop = stop->getNextStop();
        }
    }

    updateWidget();
}

void GradientWithStops::updateWidget() {
    update();
}

void GradientWithStops::loadCursors() {
    if (_cursorsLoaded) return;

    double dpr = devicePixelRatio();
    _cursorMouseover = std::make_unique<QCursor>(load_svg_cursor_qt("gradient-over-stop.svg", dpr));
    _cursorDragging = std::make_unique<QCursor>(load_svg_cursor_qt("gradient-drag-stop.svg", dpr));
    _cursorInsert = std::make_unique<QCursor>(load_svg_cursor_qt("gradient-add-stop.svg", dpr));
    _cursorsLoaded = true;
}

// return on-screen position of the UI stop corresponding to the gradient's color stop at 'index'
GradientWithStops::StopPos GradientWithStops::getStopPosition(size_t index, const Layout& layout) const {
    if (!_gradient || index >= _stops.size()) {
        return StopPos {};
    }

    // half of the stop template width; round it to avoid half-pixel coordinates
    const auto dx = round(_template.get_width_px() / 2);

    auto pos = [&](double offset) { return round(layout.x + layout.width * std::clamp(offset, 0.0, 1.0)); };
    const auto& v = _stops;

    auto offset = pos(v[index].offset);
    auto left = offset - dx;
    if (index > 0) {
        // check previous stop; it may overlap
        auto prev = pos(v[index - 1].offset) + dx;
        if (prev > left) {
            // overlap
            left = round((left + prev) / 2);
        }
    }

    auto right = offset + dx;
    if (index + 1 < v.size()) {
        // check next stop for overlap
        auto next = pos(v[index + 1].offset) - dx;
        if (right > next) {
            // overlap
            right = round((right + next) / 2);
        }
    }

    return StopPos {
        .left = left,
        .tip = offset,
        .right = right,
        .top = layout.height - _template.get_height_px(),
        .bottom = layout.height
    };
}

// widget's layout; mainly location of the gradient's image and stop handles
GradientWithStops::Layout GradientWithStops::getLayout() const {
    const double stopWidth = _template.get_width_px();
    const double halfStop = round(stopWidth / 2);
    const double x = halfStop;
    const double width = this->width() - stopWidth;
    const double height = this->height();

    return Layout {
        .x = x,
        .y = 0,
        .width = width,
        .height = height
    };
}

// check if stop handle is under (x, y) location, return its index or -1 if not hit
int GradientWithStops::findStopAt(double x, double y) const {
    if (!_gradient) return -1;

    const auto& v = _stops;
    const auto& layout = getLayout();

    // find stop handle at (x, y) position; note: stops may not be ordered by offsets
    for (size_t i = 0; i < v.size(); ++i) {
        auto pos = getStopPosition(i, layout);
        if (x >= pos.left && x <= pos.right && y >= pos.top && y <= pos.bottom) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

// this is range of offset adjustment for a given stop
GradientWithStops::Limits GradientWithStops::getStopLimits(int maybeIndex) const {
    if (!_gradient) return Limits {};

    // let negative index turn into a large out-of-range number
    auto index = static_cast<size_t>(maybeIndex);

    const auto& v = _stops;

    if (index < v.size()) {
        double min = 0;
        double max = 1;

        if (v.size() > 1) {
            std::vector<double> offsets;
            offsets.reserve(v.size());
            for (auto& s : _stops) {
                offsets.push_back(s.offset);
            }
            std::sort(offsets.begin(), offsets.end());

            // special cases:
            if (index == 0) { // first stop
                max = offsets[index + 1];
            }
            else if (index + 1 == v.size()) { // last stop
                min = offsets[index - 1];
            }
            else {
                // stops "inside" gradient
                min = offsets[index - 1];
                max = offsets[index + 1];
            }
        }
        return Limits { .minOffset = min, .maxOffset = max, .offset = v[index].offset };
    }
    else {
        return Limits {};
    }
}

bool GradientWithStops::focusNextPrevChild(bool next) {
    // On arrow key, let ::keyPressEvent move focused stop (horz) / nothing (vert)
    // Only handle tab navigation
    if (!(hasFocus() || QApplication::focusWidget() == this)) {
        return QWidget::focusNextPrevChild(next);
    }

    auto nStops = _stops.size();
    auto backward = !next;

    if (hasFocus()) {
        auto newStop = _focusedStop + (backward ? -1 : +1);
        // out of range: keep _focusedStop, but give up focus on widget overall
        if (!(newStop >= 0 && static_cast<size_t>(newStop) < nStops)) {
            return false; // let focus go
        }
        // in range: next/prev stop
        setFocusedStop(newStop);
    } else {
        // didn't have focus: grab on 1st or last stop, relevant to direction
        setFocus();
        if (nStops > 0) { // ...unless we have no stop, then just focus widget
            setFocusedStop(backward ? static_cast<int>(nStops - 1) : 0);
        }
    }

    return true;
}

void GradientWithStops::keyPressEvent(QKeyEvent* event) {
    // currently all keyboard activity involves acting on focused stop handle; bail if nothing's selected
    if (_focusedStop < 0) {
        QWidget::keyPressEvent(event);
        return;
    }

    auto delta = _stopMoveIncrement;
    if (event->modifiers() & Qt::ShiftModifier) {
        delta *= 10;
    }

    switch (event->key()) {
        case Qt::Key_Left:
            moveStop(_focusedStop, -delta);
            event->accept();
            return;

        case Qt::Key_Right:
            moveStop(_focusedStop, delta);
            event->accept();
            return;

        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            Q_EMIT deleteStop(_focusedStop);
            event->accept();
            return;
    }

    QWidget::keyPressEvent(event);
}

void GradientWithStops::mousePressEvent(QMouseEvent* event) {
    if (!_gradient) return;

    if (event->button() == Qt::LeftButton) {
        // single button press selects stop and can start dragging it

        if (!hasFocus()) {
            // grab focus, so we can show selection indicator and move selected stop with left/right keys
            setFocus();
        }

        // find stop handle
        auto index = findStopAt(event->position().x(), event->position().y());

        if (index < 0) {
            setFocusedStop(-1); // no stop
            return;
        }

        setFocusedStop(index);

        // check if clicked stop can be moved
        auto limits = getStopLimits(index);
        if (limits.minOffset < limits.maxOffset) {
            // TODO: to facilitate selecting stops without accidentally moving them,
            // delay dragging mode until mouse cursor moves certain distance...
            _dragging = true;
            _pointerX = event->position().x();
            _stopOffset = _stops.at(index).offset;

            if (_cursorDragging) {
                setStopCursor(_cursorDragging.get());
            }
        }
    }
}

void GradientWithStops::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !_dragging) {
        // double-click may insert a new stop (simplified logic)
        auto index = findStopAt(event->position().x(), event->position().y());
        if (index >= 0) return;

        auto layout = getLayout();
        if (layout.width > 0 && event->position().x() > layout.x && event->position().x() < layout.x + layout.width) {
            auto position = (event->position().x() - layout.x) / layout.width;
            // request new stop
            Q_EMIT addStopAt(position);
        }
    } else {
        setStopCursor(getCursor(event->position().x(), event->position().y()));
    }
    _dragging = false;
}

// move stop by a given amount (delta)
void GradientWithStops::moveStop(int stopIndex, double offsetShift) {
    auto layout = getLayout();
    if (layout.width > 0) {
        auto limits = getStopLimits(stopIndex);
        if (limits.minOffset < limits.maxOffset) {
            auto newOffset = std::clamp(limits.offset + offsetShift, limits.minOffset, limits.maxOffset);
            if (newOffset != limits.offset) {
                Q_EMIT stopOffsetChanged(stopIndex, newOffset);
            }
        }
    }
}

void GradientWithStops::mouseMoveEvent(QMouseEvent* event) {
    if (!_gradient) return;

    auto drag = event->buttons() & Qt::LeftButton;
    if (!drag) _dragging = false;

    if (_dragging) {
        // move stop to a new position (adjust offset)
        auto dx = event->position().x() - _pointerX;
        auto layout = getLayout();
        if (layout.width > 0) {
            auto delta = dx / layout.width;
            auto limits = getStopLimits(_focusedStop);
            if (limits.minOffset < limits.maxOffset) {
                auto newOffset = std::clamp(_stopOffset + delta, limits.minOffset, limits.maxOffset);
                Q_EMIT stopOffsetChanged(_focusedStop, newOffset);
            }
        }
    } else { // !drag but may need to change cursor
        setStopCursor(getCursor(event->position().x(), event->position().y()));
    }
}

const QCursor* GradientWithStops::getCursor(double x, double y) const {
    if (!_gradient) return nullptr;

    // check if mouse if over stop handle that we can adjust
    auto index = findStopAt(x, y);
    if (index >= 0) {
        auto limits = getStopLimits(index);
        if (limits.minOffset < limits.maxOffset && _cursorMouseover) {
            return _cursorMouseover.get();
        }
    } else if (_cursorInsert) {
        return _cursorInsert.get();
    }

    return nullptr;
}

void GradientWithStops::setStopCursor(const QCursor* cursor) {
    if (_cursorCurrent == cursor) return;

    if (cursor != nullptr) {
        setCursor(*cursor);
    } else {
        unsetCursor();
    }

    _cursorCurrent = cursor;
}

void GradientWithStops::enterEvent(QEnterEvent* /*event*/) {
    loadCursors();
}

void GradientWithStops::leaveEvent(QEvent* /*event*/) {
    setStopCursor(nullptr);
}

void GradientWithStops::focusInEvent(QFocusEvent* /*event*/) {
    updateWidget();
}

void GradientWithStops::focusOutEvent(QFocusEvent* /*event*/) {
    updateWidget();
}

void GradientWithStops::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const double scale = devicePixelRatio();
    const auto layout = getLayout();

    if (layout.width <= 0) return;

    auto grad = layout;
    grad.x -= 1;
    grad.width += 2;
    int radius = 2;

    // Draw rounded rectangle and clip using QPainterPath
    QRectF gradRect(grad.x, grad.y, grad.width, GRADIENT_IMAGE_HEIGHT);
    QPainterPath clipPath = Inkscape::Util::rounded_rectangle_path(gradRect, radius);
    painter.setClipPath(clipPath);

    // Draw gradient
    drawGradient(painter, _gradient, gradRect, static_cast<int>(CHECKERBOARD_TILE));

    painter.setClipping(false);

    bool dark = Linea::UI::isDarkPalette(*this);
    Linea::UI::drawStandardBorder(painter, gradRect, dark, radius, scale, false, true);

    if (!_gradient) return;

    // draw stop handles
    auto fg = _foregroundColor;
    auto bg = _backgroundColor;

    // stop handle outlines and selection indicator use theme colors:
    _template.set_style(".outer", "fill", fg.name().toStdString());
    _template.set_style(".inner", "stroke", bg.name().toStdString());
    _template.set_style(".hole", "fill", bg.name().toStdString());

    auto tip = _tipTemplate.render_qimage(scale);

    for (size_t i = 0; i < _stops.size(); ++i) {
        const auto& stop = _stops[i];

        // stop handle shows stop color and opacity:
        _template.set_style(".color", "fill", stop.color.toString(false));
        _template.set_style(".opacity", "opacity", std::to_string(stop.opacity));

        // show/hide selection indicator
        const auto isSelected = _focusedStop == static_cast<int>(i);
        _template.set_style(".selected", "opacity", std::to_string(isSelected ? 1 : 0));

        // render stop handle
        auto pix = _template.render_qimage(scale);

        if (pix.isNull()) {
            qWarning("Rendering gradient stop failed.");
            break;
        }

        auto pos = getStopPosition(i, layout);

        // selected handle sports a 'tip' to make it easily noticeable
        if (isSelected && !tip.isNull()) {
            // paint tip bitmap at top of stop
            int tipX = static_cast<int>(round(pos.tip - tip.width() / (2.0 * scale)));
            int tipY = static_cast<int>(round(layout.y));
            painter.drawImage(tipX, tipY, tip);
        }

        // calc space available for stop marker
        int pixX = static_cast<int>(round(pos.tip - pix.width() / (2.0 * scale)));
        int pixY = static_cast<int>(round(pos.top));

        painter.save();
        painter.setClipRect(QRectF(pos.left, layout.y, pos.right - pos.left, layout.height));
        painter.drawImage(pixX, pixY, pix);
        painter.restore();
    }
}

void GradientWithStops::drawGradient(QPainter& painter, SPGradient* gradient, const QRectF& rect, int tileSize) {
    if (!gradient) {
        // Draw checkerboard pattern
        int tile = tileSize > 0 ? tileSize : 6;
        QBrush brush(checkerPattern(QApplication::palette().color(QPalette::Window), tile));
        painter.setBrushOrigin(rect.topLeft().toPoint());
        painter.fillRect(rect, brush);
        return;
    }

    // Create Cairo surface and context, then use the existing draw_gradient function
    int width = static_cast<int>(std::ceil(rect.width()));
    int height = static_cast<int>(std::ceil(rect.height()));

    if (width <= 0 || height <= 0) return;

    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, width, height);
    auto cr = Cairo::Context::create(surface);

    // Draw rectangle first (like GTK version does)
    cr->rectangle(0, 0, width, height);

    // Ensure gradient vector is built before drawing
    gradient->ensureVector();

    // Draw the gradient using the existing function
    ::draw_gradient(cr, gradient, 0, width, tileSize);

    // Convert to QImage and draw
    QImage image = Inkscape::UI::cairo_surface_to_qimage(surface->cobj());
    if (!image.isNull()) {
        painter.drawImage(static_cast<int>(rect.x()), static_cast<int>(rect.y()), image);
    }
}

// focused/selected stop indicator
void GradientWithStops::setFocusedStop(int index) {
    if (_focusedStop == index) return;

    _focusedStop = index;
    Q_EMIT stopSelected(index);
    updateWidget();
}

} // namespace Linea::UI
