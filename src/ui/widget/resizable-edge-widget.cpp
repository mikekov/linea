// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ResizableEdgeWidget — base widget for drag-to-resize along one or more edges.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 */

#include "ui/widget/resizable-edge-widget.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVariantAnimation>

namespace Linea::UI {

namespace {

constexpr Qt::Edge ALL_EDGES[] = {Qt::LeftEdge, Qt::TopEdge, Qt::RightEdge, Qt::BottomEdge};

// crossfade tempo, matching the number-edit hover controls
constexpr int HANDLE_FADE_IN_MS = 70;
constexpr int HANDLE_FADE_OUT_MS = 200;

// The invisible drag strip along a resizable edge. Fades a 1px accent line
// in at the panel's boundary when hovered — the strip itself is the hit
// zone, so its enter/leave is exactly "over the resizable edge". Mouse
// presses are left unhandled so they propagate to the parent for resizing.
class ResizeHandle : public QWidget {
public:
    ResizeHandle(Qt::Edge edge, QWidget* parent)
        : QWidget(parent)
        , _edge(edge) {
        QObject::connect(&_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            _opacity = v.toDouble();
            update();
        });
    }

    // the highlight is opt-in on the parent widget; when off, hover and
    // drag state is still tracked but the line never fades in
    void setHighlight(bool on) {
        _highlight = on;
        if (!on) {
            _fade.stop();
            _opacity = 0;
            update();
        } else {
            fade(_dragged || _hovered);
        }
    }

    // keep the edge lit for the whole drag — the cursor leaves the strip
    // the moment the panel starts resizing
    void setDragged(bool dragged) {
        _dragged = dragged;
        fade(_dragged || _hovered);
    }

protected:
    void enterEvent(QEnterEvent*) override {
        _hovered = true;
        fade(true);
    }

    void leaveEvent(QEvent*) override {
        _hovered = false;
        if (!_dragged) {
            fade(false);
        }
    }

    void paintEvent(QPaintEvent*) override {
        if (_opacity <= 0 || !_highlight) return;
        QPainter p(this);
        p.setOpacity(_opacity);
        p.fillRect(edgeLine(), palette().color(QPalette::Mid));
    }

private:
    void fade(bool show) {
        if (!_highlight) return;
        // reconfiguring a stopped animation re-emits valueChanged at the
        // stale currentTime — that would corrupt _opacity before start()
        {
            const QSignalBlocker blocker(&_fade);
            _fade.stop();
            _fade.setDuration(show ? HANDLE_FADE_IN_MS : HANDLE_FADE_OUT_MS);
            _fade.setStartValue(_opacity);
            _fade.setEndValue(show ? 1.0 : 0.0);
        }
        _fade.start();
    }

    // the parent's boundary line is this strip's outermost row/column
    QRect edgeLine() const {
        switch (_edge) {
            case Qt::LeftEdge:   return {0, 0, 1, height()};
            case Qt::RightEdge:  return {width() - 1, 0, 1, height()};
            case Qt::TopEdge:    return {0, 0, width(), 1};
            case Qt::BottomEdge: return {0, height() - 1, width(), 1};
        }
        return {};
    }

    Qt::Edge _edge;
    qreal _opacity = 0;
    bool _hovered = false;
    bool _dragged = false;
    bool _highlight = false;
    QVariantAnimation _fade;
};

int edgeIndex(Qt::Edge edge) {
    switch (edge) {
        case Qt::LeftEdge:   return 0;
        case Qt::TopEdge:    return 1;
        case Qt::RightEdge:  return 2;
        case Qt::BottomEdge: return 3;
    }
    return -1;
}

} // namespace

ResizableEdgeWidget::ResizableEdgeWidget(QWidget* parent)
    : QWidget(parent) {
    // needed to update the cursor over corner zones, which are not
    // covered by the per-edge handle widgets
    setMouseTracking(true);
}

void ResizableEdgeWidget::setResizableEdges(Qt::Edges edges) {
    _resizableEdges = edges & (Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge);
    updateResizeHandle();
}

Qt::Edges ResizableEdgeWidget::resizableEdges() const {
    return _resizableEdges;
}

void ResizableEdgeWidget::setResizableEdge(Qt::Edge edge) {
    setResizableEdges(Qt::Edges(edge));
}

void ResizableEdgeWidget::setEdgeHighlight(bool on) {
    _edgeHighlight = on;
    for (auto handle : _handles) {
        if (handle) {
            static_cast<ResizeHandle*>(handle)->setHighlight(on);
        }
    }
}

bool ResizableEdgeWidget::edgeHighlight() const {
    return _edgeHighlight;
}

void ResizableEdgeWidget::setResizeStep(const QSize& step) {
    _resizeStep = step;
}

QSize ResizableEdgeWidget::resizeStep() const {
    return _resizeStep;
}

void ResizableEdgeWidget::setSymmetricResizeAxes(Qt::Orientations axes) {
    _symmetricAxes = axes & (Qt::Horizontal | Qt::Vertical);
}

Qt::Orientations ResizableEdgeWidget::symmetricResizeAxes() const {
    return _symmetricAxes;
}

QWidget* ResizableEdgeWidget::handleWidget(Qt::Edge edge) {
    const int i = edgeIndex(edge);
    if (i < 0) {
        return nullptr;
    }
    if (!_handles[i]) {
        _handles[i] = new ResizeHandle(edge, this);
        _handles[i]->setCursor(cursorForEdges(Qt::Edges(edge)));
        _handles[i]->setProperty("class", "resize-handle");
        static_cast<ResizeHandle*>(_handles[i])->setHighlight(_edgeHighlight);
    }
    return _handles[i];
}

void ResizableEdgeWidget::updateResizeHandle() {
    const int w = width();
    const int h = height();
    const int s = RESIZE_HANDLE_WIDTH;

    // corner squares are only carved out of a handle's span when the
    // perpendicular edge is also enabled; there the parent does the
    // diagonal hit test
    const int topInset    = (_resizableEdges & Qt::TopEdge)    ? s : 0;
    const int bottomInset = (_resizableEdges & Qt::BottomEdge) ? s : 0;
    const int leftInset   = (_resizableEdges & Qt::LeftEdge)   ? s : 0;
    const int rightInset  = (_resizableEdges & Qt::RightEdge)  ? s : 0;

    for (Qt::Edge edge : ALL_EDGES) {
        const int i = edgeIndex(edge);
        if (!(_resizableEdges & edge) || !canResize()) {
            if (_handles[i]) {
                _handles[i]->hide();
            }
            continue;
        }

        auto handle = handleWidget(edge);
        switch (edge) {
            case Qt::LeftEdge:
                handle->setGeometry(0, topInset, s, h - topInset - bottomInset);
                break;
            case Qt::RightEdge:
                handle->setGeometry(w - s, topInset, s, h - topInset - bottomInset);
                break;
            case Qt::TopEdge:
                handle->setGeometry(leftInset, 0, w - leftInset - rightInset, s);
                break;
            case Qt::BottomEdge:
                handle->setGeometry(leftInset, h - s, w - leftInset - rightInset, s);
                break;
        }
        handle->show();
        handle->raise();
    }
}

Qt::Edges ResizableEdgeWidget::edgesAt(QPoint pos) const {
    Qt::Edges edges;
    if (!canResize()) {
        return edges;
    }
    const int s = RESIZE_HANDLE_WIDTH;
    if ((_resizableEdges & Qt::LeftEdge)   && pos.x() < s) {
        edges |= Qt::LeftEdge;
    }
    if ((_resizableEdges & Qt::RightEdge)  && pos.x() >= width() - s) {
        edges |= Qt::RightEdge;
    }
    if ((_resizableEdges & Qt::TopEdge)    && pos.y() < s) {
        edges |= Qt::TopEdge;
    }
    if ((_resizableEdges & Qt::BottomEdge) && pos.y() >= height() - s) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

QCursor ResizableEdgeWidget::cursorForEdges(Qt::Edges edges) {
    const bool horiz = edges & (Qt::LeftEdge | Qt::RightEdge);
    const bool vert  = edges & (Qt::TopEdge | Qt::BottomEdge);
    if (horiz && vert) {
        const bool fdiag = (edges & (Qt::TopEdge | Qt::LeftEdge)) == (Qt::TopEdge | Qt::LeftEdge) ||
                           (edges & (Qt::BottomEdge | Qt::RightEdge)) == (Qt::BottomEdge | Qt::RightEdge);
        return fdiag ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
    }
    if (horiz) {
        return Qt::SplitHCursor;
    }
    if (vert) {
        return Qt::SplitVCursor;
    }
    return Qt::ArrowCursor;
}

QSize ResizableEdgeWidget::snapAxes(QSize newSize, Qt::Edges edges) const {
    const int sx = _resizeStep.width();
    const int sy = _resizeStep.height();
    // on symmetric axes the step is applied to both sides at once,
    // so the size advances by 2 steps
    if (sx > 1 && (edges & (Qt::LeftEdge | Qt::RightEdge))) {
        // snap to the closest multiple of resizing step
        const int step = (_symmetricAxes & Qt::Horizontal) ? 2 * sx : sx;
        newSize.setWidth(((newSize.width() + step / 2) / step) * step);
    }
    if (sy > 1 && (edges & (Qt::TopEdge | Qt::BottomEdge))) {
        const int step = (_symmetricAxes & Qt::Vertical) ? 2 * sy : sy;
        newSize.setHeight(((newSize.height() + step / 2) / step) * step);
    }
    return newSize;
}

QSize ResizableEdgeWidget::snapResize(QSize newSize) const {
    // only snap the axes actually being dragged
    return snapAxes(newSize, _dragEdges);
}

void ResizableEdgeWidget::mousePressEvent(QMouseEvent* event) {
    const Qt::Edges edges = edgesAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && edges) {
        _resizing = true;
        _dragEdges = edges;
        _dragStartPos = event->globalPosition().toPoint();
        _dragStartSize = size();
        setCursor(cursorForEdges(edges));
        for (Qt::Edge edge : ALL_EDGES) {
            if (_dragEdges & edge) {
                static_cast<ResizeHandle*>(handleWidget(edge))->setDragged(true);
            }
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ResizableEdgeWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!_resizing) {
        // update the hover cursor (handle widgets carry their own cursors,
        // so this matters mainly for corner zones)
        const Qt::Edges edges = edgesAt(event->position().toPoint());
        if (edges) {
            setCursor(cursorForEdges(edges));
        } else {
            unsetCursor();
        }
        return;
    }

    const QPoint delta = event->globalPosition().toPoint() - _dragStartPos;
    QSize newSize = _dragStartSize;

    // when the layout keeps the widget centered on an axis, a size change moves
    // the dragged edge by only half the delta; apply it twice so both sides
    // move together and the edge tracks the cursor
    const int dx = (_symmetricAxes & Qt::Horizontal) ? 2 * delta.x() : delta.x();
    const int dy = (_symmetricAxes & Qt::Vertical) ? 2 * delta.y() : delta.y();

    if (_dragEdges & Qt::LeftEdge) {
        newSize.rwidth() -= dx;
    }
    if (_dragEdges & Qt::RightEdge) {
        newSize.rwidth() += dx;
    }
    if (_dragEdges & Qt::TopEdge) {
        newSize.rheight() -= dy;
    }
    if (_dragEdges & Qt::BottomEdge) {
        newSize.rheight() += dy;
    }

    // respect min/max size, then snap to increments
    newSize = newSize.expandedTo(minimumSize()).boundedTo(maximumSize());
    newSize = snapResize(newSize);

    resize(newSize);
    updateResizeHandle();
    Q_EMIT resized(newSize);
    event->accept();
}

void ResizableEdgeWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (_resizing) {
        _resizing = false;
        _dragEdges = {};
        unsetCursor();
        for (auto handle : _handles) {
            if (handle) {
                static_cast<ResizeHandle*>(handle)->setDragged(false);
            }
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ResizableEdgeWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateResizeHandle();
}

} // namespace Linea::UI
