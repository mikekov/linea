// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DrawingArea — reusable widget for custom drawing with callback
 */

#include "drawing-area.h"

#include <QPainter>

namespace Linea::UI {

DrawingArea::DrawingArea(QWidget* parent)
    : QWidget(parent)
{
}

DrawingArea::~DrawingArea() = default;

void DrawingArea::setDrawCallback(DrawCallback callback) {
    _drawCallback = std::move(callback);
}

void DrawingArea::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    if (_drawCallback) {
        QPainter painter(this);
        _drawCallback(&painter, event->rect());
    }
}

} // namespace Linea::UI
