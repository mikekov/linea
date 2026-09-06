// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ResizableEdgeWidget — base widget for one-edge horizontal resizing.
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

ResizableEdgeWidget::ResizableEdgeWidget(QWidget* parent)
    : QWidget(parent) {
    _resizeHandle = new QWidget(this);
    _resizeHandle->setFixedWidth(RESIZE_HANDLE_WIDTH);
    _resizeHandle->setCursor(Qt::SplitHCursor);
    _resizeHandle->hide();
    _resizeHandle->setProperty("class", "resize-handle");
}

void ResizableEdgeWidget::setResizableEdge(Qt::Edge edge) {
    _resizableEdge = edge;
    updateResizeHandle();
}

Qt::Edge ResizableEdgeWidget::resizableEdge() const {
    return _resizableEdge;
}

void ResizableEdgeWidget::updateResizeHandle() {
    if (_resizableEdge == Qt::Edge(0) || !canResize()) {
        _resizeHandle->hide();
        return;
    }

    _resizeHandle->show();
    int handleX = _resizableEdge == Qt::LeftEdge ? 0 : width() - RESIZE_HANDLE_WIDTH;
    _resizeHandle->setGeometry(
        handleX,
        0,
        RESIZE_HANDLE_WIDTH,
        height()
    );
    _resizeHandle->raise();
}

void ResizableEdgeWidget::mousePressEvent(QMouseEvent* event) {
    if (_resizableEdge == Qt::Edge(0) || !canResize()) {
        QWidget::mousePressEvent(event);
        return;
    }

    int handleX = _resizableEdge == Qt::LeftEdge ? 0 : width() - RESIZE_HANDLE_WIDTH;
    if (event->position().x() >= handleX && event->position().x() <= handleX + RESIZE_HANDLE_WIDTH) {
        _resizing = true;
        _dragStartX = event->globalPosition().x();
        _dragStartWidth = width();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ResizableEdgeWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!_resizing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int deltaX = event->globalPosition().x() - _dragStartX;
    int newWidth = _dragStartWidth;

    if (_resizableEdge == Qt::LeftEdge) {
        newWidth -= deltaX;
    } else {
        newWidth += deltaX;
    }

    // Respect min/max width
    newWidth = qBound(minimumWidth(), newWidth, maximumWidth());

    // Snap to subclass-defined increments
    newWidth = snapResizeWidth(newWidth);

    resize(newWidth, height());
    updateResizeHandle();
    Q_EMIT resized(newWidth);
    event->accept();
}

void ResizableEdgeWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (_resizing) {
        _resizing = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ResizableEdgeWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateResizeHandle();
}

} // namespace Linea::UI
