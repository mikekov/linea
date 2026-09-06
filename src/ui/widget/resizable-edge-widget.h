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

#ifndef LINEA_RESIZABLE_EDGE_WIDGET_H
#define LINEA_RESIZABLE_EDGE_WIDGET_H

#include <QWidget>

class QMouseEvent;
class QResizeEvent;

namespace Linea::UI {

/**
 * Base widget providing a resize handle along the left or right edge.
 *
 * Subclasses can override canResize() to disable resizing and
 * snapResizeWidth() to snap the new width to desired increments.
 */
class ResizableEdgeWidget : public QWidget {
    Q_OBJECT

public:
    explicit ResizableEdgeWidget(QWidget* parent = nullptr);

    void setResizableEdge(Qt::Edge edge);
    Qt::Edge resizableEdge() const;

Q_SIGNALS:
    void resized(int width);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    virtual bool canResize() const { return true; }
    virtual int snapResizeWidth(int newWidth) const { return newWidth; }

    void updateResizeHandle();

private:
    QWidget* _resizeHandle = nullptr;
    Qt::Edge _resizableEdge = Qt::Edge(0);
    bool _resizing = false;
    int _dragStartX = 0;
    int _dragStartWidth = 0;
    static constexpr int RESIZE_HANDLE_WIDTH = 5;
};

} // namespace Linea::UI

#endif // LINEA_RESIZABLE_EDGE_WIDGET_H
