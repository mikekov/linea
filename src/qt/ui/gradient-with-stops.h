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

#ifndef LINEA_UI_GRADIENT_WITH_STOPS_H
#define LINEA_UI_GRADIENT_WITH_STOPS_H

#include <QWidget>
#include <QCursor>
#include <vector>
#include <memory>
#include <sigc++/scoped_connection.h>

#include "colors/color.h"
#include "ui/svg-renderer.h"

class SPGradient;

namespace Linea::UI {

/**
 * Widget that displays a gradient with draggable stop handles.
 * Allows users to select, move, add, and delete gradient stops.
 */
class GradientWithStops : public QWidget {
    Q_OBJECT

public:
    GradientWithStops(QWidget* parent = nullptr);
    ~GradientWithStops() override;

    // gradient to draw or nullptr
    void setGradient(SPGradient* gradient);

    // set selected stop handle (or pass -1 to deselect)
    void setFocusedStop(int index);

Q_SIGNALS:
    // stop has been selected
    void stopSelected(size_t index);

    // request to change stop's offset
    void stopOffsetChanged(size_t index, double offset);

    // request to add a new stop at given offset (0.0-1.0)
    void addStopAt(double offset);

    // request to delete stop at given index
    void deleteStop(size_t index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void modified();
    void updateWidget();

    // index of gradient stop handle under (x, y) or -1
    int findStopAt(double x, double y) const;

    // request stop move
    void moveStop(int stopIndex, double offsetShift);

    // layout of gradient image/editor
    struct Layout {
        double x, y, width, height;
    };
    Layout getLayout() const;

    // position of single gradient stop handle
    struct StopPos {
        double left, tip, right, top, bottom;
    };
    StopPos getStopPosition(size_t index, const Layout& layout) const;

    struct Limits {
        double minOffset, maxOffset, offset;
    };
    Limits getStopLimits(int index) const;

    const QCursor* getCursor(double x, double y) const;
    void setStopCursor(const QCursor* cursor);
    void loadCursors();

    // drawing helpers
    void drawGradient(QPainter& painter, SPGradient* gradient, const QRectF& rect, int tileSize);
    void drawStopHandles(QPainter& painter, const Layout& layout, double scale);

    SPGradient* _gradient = nullptr;

    struct Stop {
        double offset;
        Inkscape::Colors::Color color;
        double opacity;
    };
    std::vector<Stop> _stops;

    // handle stop SVG template
    Inkscape::svg_renderer _template;
    Inkscape::svg_renderer _tipTemplate;

    // selected handle indicator
    int _focusedStop = -1;
    bool _dragging = false;
    double _pointerX = 0;
    double _stopOffset = 0;

    // cursors
    std::unique_ptr<QCursor> _cursorMouseover;
    std::unique_ptr<QCursor> _cursorDragging;
    std::unique_ptr<QCursor> _cursorInsert;
    const QCursor* _cursorCurrent = nullptr;
    bool _cursorsLoaded = false;

    // connections (using sigc for SPGradient signals)
    sigc::scoped_connection _release;
    sigc::scoped_connection _modified;

    // styling
    QColor _backgroundColor;
    QColor _foregroundColor;

    // TODO: customize this amount or read prefs
    double _stopMoveIncrement = 0.01;

    // constants
    static constexpr int CHECKERBOARD_TILE = 7;
    static constexpr int GRADIENT_IMAGE_HEIGHT = 3 * CHECKERBOARD_TILE;
};

} // namespace Linea::UI

#endif // LINEA_UI_GRADIENT_WITH_STOPS_H
