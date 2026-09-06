// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * SimpleList — virtual list widget that arranges rows top to bottom.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_SIMPLE_LIST_H
#define LINEA_UI_SIMPLE_LIST_H

#include <cstdint>
#include <QAbstractScrollArea>
#include <functional>
#include <vector>

namespace Linea::UI {

// Forward declaration
class SimpleListViewport;

/**
 * Simple "virtual" list that arranges rows one after another and delegates
 * row ownership and drawing to a client.
 * It is lightweight and can handle millions of rows.
 * It provides no caching.
 * It can track one row (selected one) and tell when it changes.
 *
 * The widget does not know the exact height of any row ahead of time. The
 * draw callback is the only source of row heights: it is called for rows that
 * are about to be drawn and returns the height that was actually used. Heights
 * are only remembered for the rows currently visible in the viewport.
 *
 * Vertical scrolling is percentage-based: the scroll bar position is a
 * fraction of the total row count. In the top half of the list rows are drawn
 * top-down; in the bottom half they are drawn bottom-up so that both ends of
 * the list remain reachable without depending on the (unknown) average row
 * height.
 */
class SimpleList : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit SimpleList(QWidget* parent = nullptr);
    ~SimpleList() override;

    // default/average row height used only for the initial page-step estimate
    void setAverageRowHeight(int height);
    int averageRowHeight() const;
    // set vertical gap between rows to allow drawing separating lines
    void setRowGap(int gap);
    int rowGap() const;
    // if true, paint row gaps with a separator color
    void setShowGap(bool show);
    // should rows be selectable?
    void setSelectable(bool isSelectable);
    // add or remove a frame around the list
    void setHasFrame(bool frame = true);
    // total number of rows to present in the list
    void setRowCount(std::size_t count);
    std::size_t rowCount() const;
    // repaint the entire list after rows have changed
    void invalidate();
    // remove rows, clear the list
    void clear();
    // select a row by index
    void setSelectedRow(int index);
    // currently selected row index, or -1 if none
    int selectedRow() const;

    // register callback to draw a row. The function receives the painter, the row
    // index, the y anchor, the available width and the selected state. The y
    // anchor is the top of the row when drawing top-down, or the bottom of the
    // row when drawing bottom-up. The function must draw the row and return the
    // height it actually occupied.
    using DrawFunc = std::function<int (QPainter* painter, std::uint32_t index, int y, int width, bool selected, bool topDown)>;
    void setDrawFunc(DrawFunc func);

    // register callback to get tooltip text for a row
    using TooltipFunc = std::function<QString (int index)>;
    void setTooltipFunc(TooltipFunc func);

Q_SIGNALS:
    // emitted when selected row has changed
    void rowSelected(int index);
    // emitted when the user tries to "open" a row by double-clicking or pressing Enter key
    void rowOpened(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    class SimpleListViewport : public QWidget {
    public:
        SimpleListViewport(SimpleList* parent);
        void paintEvent(QPaintEvent* event) override;
        bool event(QEvent* event) override;

    private:
        SimpleList* _list;
    };

    struct VisibleRow {
        int index = -1;
        int height = 0;
        int y = 0; // top y in viewport coordinates
    };

    void construct();
    void drawContent(QPainter* painter, int width, int height);
    int drawRow(QPainter* painter, int index, int y, int width, bool topDown);
    bool calcLayout(int width, int height);
    void resizeLayout();
    int getRowIndex(double x, double y) const;
    int getVScrollPosition() const;
    void moveSel(int delta);
    void moveSelTo(int row);
    void selectRow(int index);
    void openRow(int index);
    void scrollTo(int row);
    int pageStepRows() const;

    SimpleListViewport* _viewport;
    int _averageRowHeight = 0;
    int _gap = 0;
    bool _paintGaps = true;
    bool _hasFrame = false;
    std::size_t _rowCount = 0;
    int _selectedRow = -1;
    bool _layoutValid = false;
    int _clickedRow = -1;
    int _pageStepRows = 1;
    std::vector<VisibleRow> _visibleRows;
    DrawFunc _drawFunc;
    TooltipFunc _tooltipFunc;
};

} // namespace Linea::UI

#endif // LINEA_UI_SIMPLE_LIST_H
