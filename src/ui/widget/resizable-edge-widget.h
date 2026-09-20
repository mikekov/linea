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

#ifndef LINEA_RESIZABLE_EDGE_WIDGET_H
#define LINEA_RESIZABLE_EDGE_WIDGET_H

#include <QWidget>

class QMouseEvent;
class QResizeEvent;

namespace Linea::UI {

/**
 * Base widget providing resize handles along any combination of edges.
 *
 * Enable edges with setResizableEdges(); a drag on an edge strip resizes
 * the widget along that axis, and a drag on a corner (where two enabled
 * edges meet) resizes both axes at once.
 *
 * Subclasses can override canResize() to disable resizing and
 * snapResize() to customize snapping; the default implementation snaps
 * the dragged axes to resizeStep() increments.
 */
class ResizableEdgeWidget : public QWidget {
    Q_OBJECT

public:
    explicit ResizableEdgeWidget(QWidget* parent = nullptr);

    void setResizableEdges(Qt::Edges edges);
    Qt::Edges resizableEdges() const;
    // convenience for enabling a single edge
    void setResizableEdge(Qt::Edge edge);

    // drag increments applied per axis; a component <= 1 disables snapping on it
    void setResizeStep(const QSize& step);
    QSize resizeStep() const;

    // axes on which an edge drag grows/shrinks both sides equally; set when the
    // parent layout keeps the widget centered on that axis, so the dragged edge
    // tracks the cursor (each side moves by one resize step at a time)
    void setSymmetricResizeAxes(Qt::Orientations axes);
    Qt::Orientations symmetricResizeAxes() const;

Q_SIGNALS:
    void resized(QSize size);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    virtual bool canResize() const { return true; }
    virtual QSize snapResize(QSize newSize) const;
    // snap the given axes of a size to resizeStep() increments, independent
    // of drag state (used to snap programmatic sizes)
    QSize snapAxes(QSize newSize, Qt::Edges edges) const;

    void updateResizeHandle();

private:
    Qt::Edges edgesAt(QPoint pos) const;
    QWidget* handleWidget(Qt::Edge edge);
    static QCursor cursorForEdges(Qt::Edges edges);

    QWidget* _handles[4] = {};       ///< one handle strip per edge, lazily created
    Qt::Edges _resizableEdges;       ///< enabled edges (empty = resizing off)
    Qt::Edges _dragEdges;            ///< edges being dragged (may be a corner pair)
    bool _resizing = false;
    QPoint _dragStartPos;
    QSize _dragStartSize;
    QSize _resizeStep{1, 1};
    Qt::Orientations _symmetricAxes;
    static constexpr int RESIZE_HANDLE_WIDTH = 5;
};

} // namespace Linea::UI

#endif // LINEA_RESIZABLE_EDGE_WIDGET_H
