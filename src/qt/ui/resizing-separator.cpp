// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ResizingSeparator implementation
 *//*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2024-2026 Authors
 *
 */

#include "resizing-separator.h"

#include <algorithm>
#include <QCursor>
#include <QMouseEvent>
#include <QApplication>

namespace Linea::UI {

ResizingSeparator::ResizingSeparator(QWidget* parent, Orientation orientation)
    : QWidget(parent)
    , _orientation(orientation)
{
    setMinimumSize(_size);
    // setObjectName("ResizingSeparator");
    setProperty("class", "resizing-separator");
    setAttribute(Qt::WA_StyledBackground, true);
    // setAutoFillBackground(true);
    updateGeometry();
    updateCursor();
}

void ResizingSeparator::resize(QWidget* widget, QSize min, QSize max) {
    _resizeWidget = widget;
    _minSize = min;
    _maxSize = max;
}

void ResizingSeparator::setOrientation(Orientation orientation) {
    _orientation = orientation;
    updateGeometry();
    updateCursor();
}

void ResizingSeparator::updateGeometry() {
    if (_orientation == Orientation::Horizontal) {
        // Resizes horizontally → vertical bar, fixed thin width
        setFixedWidth(_size.width());
        // setMinimumHeight(_size.height());
        // setMaximumHeight(QWIDGETSIZE_MAX);
    } else if (_orientation == Orientation::Vertical) {
        // Resizes vertically → horizontal bar, fixed thin height
        setFixedHeight(_size.height());
        // setMinimumWidth(_size.width());
        // setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        setFixedSize(_size.width(), _size.height());
    }
}

void ResizingSeparator::updateCursor() {
    switch (_orientation) {
    case Orientation::Horizontal:
        setCursor(Qt::SizeHorCursor);
        break;
    case Orientation::Vertical:
        setCursor(Qt::SizeVerCursor);
        break;
    case Orientation::Both:
        setCursor(Qt::SizeFDiagCursor);
        break;
    }
}

void ResizingSeparator::enterEvent(QEnterEvent* event) {
    // this is needed to show the cursor when we are in a popup
    QApplication::setOverrideCursor(cursor());
    QWidget::enterEvent(event);
}

void ResizingSeparator::leaveEvent(QEvent* event) {
    QApplication::restoreOverrideCursor();
    QWidget::leaveEvent(event);
}

void ResizingSeparator::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !_resizeWidget) return;

    _dragging = true;
    _initialSize = { static_cast<double>(_resizeWidget->width()), static_cast<double>(_resizeWidget->height()) };
    _initialPosition = { event->globalPosition().x(), event->globalPosition().y() };
}

void ResizingSeparator::mouseMoveEvent(QMouseEvent* event) {
    if (!_dragging || !_resizeWidget) return;

    const auto pos  = event->globalPosition();
    const auto dist = Geom::Point(pos.x() - _initialPosition.x(),
                                  pos.y() - _initialPosition.y());
    const auto size = _initialSize + dist;

    auto clampedSize = [](double v, double min, double max) {
        return static_cast<int>(std::clamp(v, min, max > 0 ? max : 1e9));
    };

    if (_orientation == Orientation::Horizontal) {
        const int w = clampedSize(size.x(), _minSize.width(), _maxSize.width());
        _resizeWidget->setFixedWidth(w);
        Q_EMIT resized(Geom::Point(w, 0));
    } else if (_orientation == Orientation::Vertical) {
        const int h = clampedSize(size.y(), _minSize.height(), _maxSize.height());
        _resizeWidget->setFixedHeight(h);
        //TODO: verify
        // _resizeWidget->setMaximumHeight(h);
        Q_EMIT resized(Geom::Point(0, h));
    } else {
        const int w = clampedSize(size.x(), _minSize.width(), _maxSize.width());
        const int h = clampedSize(size.y(), _minSize.height(), _maxSize.height());
        _resizeWidget->resize(w, h);
        Q_EMIT resized(Geom::Point(w, h));
    }
}

void ResizingSeparator::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _dragging = false;
    }
}

} // namespace Linea::UI
