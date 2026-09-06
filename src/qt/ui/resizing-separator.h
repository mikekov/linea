// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ResizingSeparator — Draggable separator for resizing widgets
 *//*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2024-2026 Authors
 *
 */

#ifndef LINEA_UI_RESIZING_SEPARATOR_H
#define LINEA_UI_RESIZING_SEPARATOR_H

#include <QWidget>
#include <QSize>

#include "2geom/point.h"

namespace Linea::UI {

/**
 * This separator can be dragged to resize the associated widget, typically a sibling.
 *
 * Supports horizontal, vertical, or bidirectional resizing based on orientation.
 */
class ResizingSeparator : public QWidget {
    Q_OBJECT

public:
    // This is orientation of the resizing (not the widget layout)
    enum class Orientation {
        Horizontal,
        Vertical,
        Both
    };
    Q_ENUM(Orientation)

    explicit ResizingSeparator(QWidget* parent = nullptr, Orientation orientation = Orientation::Vertical);
    ~ResizingSeparator() override = default;

    /**
     * Use this separator to resize the given widget and set max size.
     * @param widget Widget to resize (typically a sibling)
     * @param min Minimum size (width, height) in pixels
     * @param max Maximum size (width, height) in pixels
     */
    void resize(QWidget* widget, QSize min, QSize max);

    /**
     * Set resizing separator orientation to decide in which direction widget resizing can occur.
     */
    void setOrientation(Orientation orientation);

Q_SIGNALS:
    /**
     * Emitted when the separator is dragged and the widget size changes.
     * @param size New size (width, height) in pixels
     */
    void resized(Geom::Point size);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updateCursor();
    void updateGeometry();

    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

    Orientation _orientation = Orientation::Vertical;
    QSize _size{4, 4};
    Geom::Point _initialPosition;
    Geom::Point _initialSize;
    QSize _minSize;
    QSize _maxSize;
    QWidget* _resizeWidget = nullptr;
    bool _dragging = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_RESIZING_SEPARATOR_H
