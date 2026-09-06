// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * SimpleGrid — virtual grid widget that arranges rectangular cells in columns and rows.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_SIMPLE_GRID_H
#define LINEA_UI_SIMPLE_GRID_H

#include <cstdint>
#include <QWidget>
#include <QScrollArea>
#include <functional>

#include "2geom/int-point.h"
#include "2geom/int-rect.h"

namespace Linea::UI {

// Forward declaration
class SimpleGridViewport;

/**
 * Simple "virtual" grid that arranges rectangular cells in columns and rows
 * and delegates cell ownership and drawing to a client.
 * It is lightweight and can handle millions of cells uniform in size.
 * It provides no caching.
 * It can track one cell (selected one) and tell when it changes.
 */
class SimpleGrid : public QWidget {
    Q_OBJECT

public:
    explicit SimpleGrid(QWidget* parent = nullptr);
    ~SimpleGrid() override;

    // establish the size of all cells in pixels
    void setCellSize(int width, int height);
    // set column and row cell gap to allow drawing separating lines
    void setGap(int gapX, int gapY);
    // if true, paint cell gaps with a separator color
    void setShowGap(bool show);
    // if true, cells will be stretched to fill up available space
    void setCellStretch(bool stretch);
    // total number of cells to present in a grid
    void setCellCount(std::size_t count);
    // should cells be selectable?
    void setSelectable(bool isSelectable);
    // add or remove a frame around the grid
    void setHasFrame(bool frame = true);
    // repaint the entire grid after cells have changed
    void invalidate(bool invalidateLayout = true);
    // remove cells, clear the grid
    void clear();
    // select a cell by index
    void setSelectedCell(int index);
    // currently selected cell index, or -1 if none
    int selectedCell() const;
    // background color
    void setActiveBackground(bool active);
    // if true, the grid widget resizes to fit all cells and scrollbars are disabled
    void setSizeToContents(bool autoSize);
    // width of the vertical scroll bar
    int verticalScrollBarWidth() const;

    // cell layout direction: Horizontal (left-to-right, top-to-bottom) or Vertical (top-to-bottom, left-to-right)
    enum class Flow { Horizontal, Vertical };
    void setFlow(Flow flow);
    Flow flow() const;

    // register callback to draw cells, one at a time, given painter, cell index, area and selected status
    using DrawFunc = std::function<void(QPainter* painter, std::uint32_t index, const Geom::IntRect& rect, bool selected)>;
    void setDrawFunc(DrawFunc func);

    // register callback to get tooltip text for a cell
    using TooltipFunc = std::function<QString(int index)>;
    void setTooltipFunc(TooltipFunc func);

Q_SIGNALS:
    // emitted when a cell is pressed
    void cellPress(int index, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons);
    // emitted when a cell is clicked
    void cellClicked(int index, Qt::KeyboardModifiers modifiers);
    // emitted when selected cell has changed
    void cellSelected(int index);
    // emitted when the user tries to "open" a cell by double-clicking or pressing Enter key
    void cellOpened(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    class SimpleGridViewport : public QWidget {
    public:
        SimpleGridViewport(SimpleGrid* parent);
        void paintEvent(QPaintEvent* event) override;
        bool event(QEvent* event) override;

    private:
        SimpleGrid* _grid;
    };

    void construct();
    void drawContent(QPainter* painter, int width, int height);
    /// Recalculate layout from the scroll area's current visible dimensions.
    void calcLayout();
    bool calcLayout(int width, int height);
    void resizeLayout();
    int getCellIndex(double x, double y) const;
    int getVScrollPosition() const;
    void moveSel(int deltaRows, int deltaCols);
    void moveSelTo(int cell);
    void selectCell(int index);
    void openCell(int index);
    void scrollTo(int cell);

    QScrollArea* _scrollArea;
    SimpleGridViewport* _viewport;
    using Size = Geom::IntPoint;
    Size _cellSize;  // requested cell size
    Size _gap;        // requested gap
    bool _paintGaps = true;
    bool _hasFrame = false;
    std::size_t _cellCount = 0; // requested number of cells in a grid
    int _selectedCell = -1;
    // layout
    Size _colsRows;    // size of grid in columns and rows
    Size _cellPitch;   // calculated cell pitch
    int _viewportRows = 0;  // number of visible rows
    int _viewportWholeRows = 0;  // number of whole rows
    bool _layoutValid = false;
    bool _stretchCells = true;
    bool _selectable = true;
    bool _sizeToContents = false;
    Flow _flow = Flow::Horizontal;
    Size _areaSize;
    int _clickedCell = -1;
    DrawFunc _drawFunc;
    TooltipFunc _tooltipFunc;
};

} // namespace Linea::UI

#endif // LINEA_UI_SIMPLE_GRID_H
