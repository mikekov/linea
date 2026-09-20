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
#include <QResizeEvent>

namespace Linea::UI {

namespace {

constexpr Qt::Edge ALL_EDGES[] = {Qt::LeftEdge, Qt::TopEdge, Qt::RightEdge, Qt::BottomEdge};

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
        _handles[i] = new QWidget(this);
        _handles[i]->setCursor(cursorForEdges(Qt::Edges(edge)));
        _handles[i]->setProperty("class", "resize-handle");
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
