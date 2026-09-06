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

#include "simple-grid.h"

#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QScrollBar>
#include <QPainter>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStyle>

#include <algorithm>
#include <cmath>

namespace Linea::UI {

SimpleGrid::SimpleGrid(QWidget* parent)
    : QWidget(parent)
{
    construct();
}

SimpleGrid::~SimpleGrid() {
    // _viewport is owned by _scrollArea, which will delete it
}

void SimpleGrid::construct() {
    setObjectName("SimpleGrid");
    setProperty("class", "SimpleGrid");
    setProperty("frame", _hasFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setWidgetResizable(true);

    _viewport = new SimpleGridViewport(this);
    // todo: find a way to make it erase background using inherited color
    _viewport->setProperty("class", "active-background");
    _scrollArea->setWidget(_viewport);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(_scrollArea);
    setHasFrame(_hasFrame);

    setFocusPolicy(Qt::StrongFocus);

    // Connect scroll bar value changed to trigger repaint
    connect(_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        _viewport->update();
    });
}

void SimpleGrid::setActiveBackground(bool active) {
    setProperty("class", active ? "active-background" : "normal-background");
    style()->unpolish(this);
    style()->polish(this);

    _viewport->setProperty("class", active ? "active-background" : "normal-background");
    style()->unpolish(_viewport);
    style()->polish(_viewport);
    _viewport->update();
}

void SimpleGrid::setSizeToContents(bool autoSize) {
    if (_sizeToContents == autoSize) return;

    _sizeToContents = autoSize;
    _scrollArea->setVerticalScrollBarPolicy(_sizeToContents ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAlwaysOn);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, _sizeToContents ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    resizeLayout();
}

int SimpleGrid::verticalScrollBarWidth() const {
    return 12;
    // this overshoots: 17px
    return style()->pixelMetric(QStyle::PixelMetric::PM_ScrollBarExtent);
    // return _scrollArea->verticalScrollBar() ? _scrollArea->verticalScrollBar()->width() : 0;
}

void SimpleGrid::setFlow(Flow flow) {
    if (_flow != flow) {
        _flow = flow;
        invalidate();
    }
}

SimpleGrid::Flow SimpleGrid::flow() const {
    return _flow;
}

SimpleGrid::SimpleGridViewport::SimpleGridViewport(SimpleGrid* parent)
    : QWidget(parent)
    , _grid(parent)
{
    setMouseTracking(true);
}

void SimpleGrid::SimpleGridViewport::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    _grid->drawContent(&painter, width(), height());
}

bool SimpleGrid::SimpleGridViewport::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        auto cell = _grid->getCellIndex(helpEvent->pos().x(), helpEvent->pos().y());
        if (cell >= 0 && _grid->_tooltipFunc) {
            QString text = _grid->_tooltipFunc(cell);
            if (!text.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos(), text, this);
                return true;
            }
        }
    }
    return QWidget::event(event);
}

void SimpleGrid::setCellSize(int width, int height) {
    auto size = Size{width, height};
    if (_cellSize != size) {
        _cellSize = size;
        invalidate();
    }
}

void SimpleGrid::setShowGap(bool show) {
    if (_paintGaps != show) {
        _paintGaps = show;
        _viewport->update();
    }
}

void SimpleGrid::setGap(int gapX, int gapY) {
    // only positive or zero gap is accepted
    auto gap = Size{std::max(0, gapX), std::max(0, gapY)};
    if (_gap != gap) {
        _gap = gap;
        invalidate();
    }
}

void SimpleGrid::setCellStretch(bool stretch) {
    _stretchCells = stretch;
    _viewport->update();
}

void SimpleGrid::setCellCount(std::size_t count) {
    _selectedCell = -1;
    if (count == 0) {
        // reset offset
        _scrollArea->verticalScrollBar()->setValue(0);
    }

    if (_cellCount != count) {
        _cellCount = count;
        invalidate();
    }
}

void SimpleGrid::setSelectable(bool isSelectable) {
    _selectable = isSelectable;
    if (!_selectable) {
        _selectedCell = -1;
        _viewport->update();
    }
}

void SimpleGrid::setHasFrame(bool frame) {
    _hasFrame = frame;
    setProperty("frame", frame);
    // setAttribute(Qt::WA_StyledBackground, true);
    style()->unpolish(this);
    style()->polish(this);
    // Reserve 1px for the frame border; no margin when frameless.
    if (auto lay = qobject_cast<QVBoxLayout*>(layout())) {
        lay->setContentsMargins(frame ? 1 : 0, frame ? 1 : 0, frame ? 1 : 0, frame ? 1 : 0);
    }
    update();
    // setProperty("class", frame ? "active-frame" : "");
    // _scrollArea->setFrameShape(frame ? QFrame::Box : QFrame::NoFrame);
}

void SimpleGrid::invalidate(bool invalidateLayout) {
    if (invalidateLayout) {
        _layoutValid = false;
    }
    // Trigger layout recalculation on next paint or resize
    _viewport->update();
}

void SimpleGrid::clear() {
    setCellCount(0);
    _viewport->update();
}

void SimpleGrid::setDrawFunc(DrawFunc func) {
    _drawFunc = std::move(func);
}

void SimpleGrid::setTooltipFunc(TooltipFunc func) {
    _tooltipFunc = std::move(func);
}

void SimpleGrid::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    resizeLayout();
}

void SimpleGrid::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Force layout recalculation when first shown to ensure correct column count
    resizeLayout();
}

void SimpleGrid::mousePressEvent(QMouseEvent* event) {
    // if (event->button() == Qt::LeftButton) {
        // Map coordinates from SimpleGrid to viewport (like GTK does)
        auto viewportPos = _scrollArea->mapFromParent(event->position().toPoint());
        _clickedCell = getCellIndex(viewportPos.x(), viewportPos.y());
        if (_clickedCell >= 0) {
            Q_EMIT cellPress(_clickedCell, event->modifiers(), event->buttons());
        }
        setFocus();
    // }
    QWidget::mousePressEvent(event);
}

void SimpleGrid::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Map coordinates from SimpleGrid to viewport (like GTK does)
        auto viewportPos = _scrollArea->mapFromParent(event->position().toPoint());
        auto index = getCellIndex(viewportPos.x(), viewportPos.y());
        if (index >= 0 && index == _clickedCell) {
            Q_EMIT cellClicked(index, event->modifiers());
            if (_selectable && _selectedCell != index) {
                selectCell(index);
            }
        }
        _clickedCell = -1;
    }
    QWidget::mouseReleaseEvent(event);
}

void SimpleGrid::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Map coordinates from SimpleGrid to viewport (like GTK does)
        auto viewportPos = _scrollArea->mapFromParent(event->position().toPoint());
        auto index = getCellIndex(viewportPos.x(), viewportPos.y());
        if (index >= 0) {
            openCell(index);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SimpleGrid::resizeLayout() {
    calcLayout();
    _viewport->update();
}

int SimpleGrid::getCellIndex(double x, double y) const {
    if (!_layoutValid || _cellPitch.x() <= 0 || _colsRows.x() <= 0) return -1;

    double width = _viewport->width();
    double height = _viewport->height();
    if (x >= 0 && y >= 0 && x < width && y < height) {
        auto columns = _colsRows.x();
        auto rows = _colsRows.y();
        auto col = _stretchCells ? std::floor(x / (width / columns)) : std::floor(x / _cellPitch.x());
        auto row = std::floor((y + getVScrollPosition()) / _cellPitch.y());
        if (col < 0 || col >= columns || row < 0 || row >= rows) return -1;

        int index = (_flow == Flow::Horizontal)
            ? static_cast<int>(row * columns + col)
            : static_cast<int>(col * rows + row);
        if (index >= 0 && index < static_cast<int>(_cellCount)) return index;
    }

    return -1;
}

int SimpleGrid::getVScrollPosition() const {
    // Qt's scroll bar value is in pixels, matching GTK's behavior
    return _scrollArea->verticalScrollBar()->value();
}

void SimpleGrid::keyPressEvent(QKeyEvent* event) {
    if (!_selectable) {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left:
        moveSel(0, -1);
        break;
    case Qt::Key_Right:
        moveSel(0, 1);
        break;
    case Qt::Key_Down:
        moveSel(1, 0);
        break;
    case Qt::Key_Up:
        moveSel(-1, 0);
        break;
    case Qt::Key_PageUp:
        moveSel(-std::max(1, _viewportWholeRows - 1), 0);
        break;
    case Qt::Key_PageDown:
        moveSel(std::max(1, _viewportWholeRows - 1), 0);
        break;
    case Qt::Key_Home:
        moveSelTo(0);
        break;
    case Qt::Key_End:
        moveSelTo(_cellCount - 1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (_selectedCell >= 0) {
            openCell(_selectedCell);
        }
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void SimpleGrid::moveSel(int deltaRows, int deltaCols) {
    if (_cellCount == 0 || !_layoutValid) return;

    int cell = _selectedCell >= 0 ? _selectedCell : 0;
    int columns = _colsRows.x();
    int rows = _colsRows.y();

    if (_flow == Flow::Vertical) {
        int max = static_cast<int>(_cellCount);
        int col = cell / rows;
        int row = cell % rows;

        auto cells_in_col = [max, rows](int c) {
            return c < max / rows ? rows : (max % rows);
        };

        if (deltaRows != 0) {
            // move up/down within the same column
            int c = col;
            int r = std::clamp(row + deltaRows, 0, std::max(0, cells_in_col(c) - 1));
            cell = c * rows + r;
        }
        else if (deltaCols != 0) {
            // move left/right, clamp to the last column that has a cell in this row
            int maxCol = (max - row - 1) / rows;
            int c = std::clamp(col + deltaCols, 0, std::max(0, maxCol));
            int r = std::clamp(row, 0, std::max(0, cells_in_col(c) - 1));
            cell = c * rows + r;
        }

        moveSelTo(cell);
        return;
    }

    if (deltaRows && deltaCols) {
        cell += deltaRows * _colsRows.x() + deltaCols;
    }
    else if (deltaRows == 0) {
        cell += deltaCols;
    }
    else {
        int delta = deltaRows * columns;
        if (delta > 0) {
            // going down
            if (cell + delta < _cellCount) {
                cell += delta;
            }
            else {
                // stop in the last row accessible from the current column
                int max = static_cast<int>(_cellCount);
                int lastRowIndex = _colsRows.y() - 1;
                int lastRowCols = max % columns;
                int currentCol = cell % columns;
                if (currentCol < lastRowCols || lastRowCols == 0) {
                    cell = lastRowIndex * columns + currentCol;
                }
                else if (lastRowIndex > 0) {
                    cell = (lastRowIndex - 1) * columns + currentCol;
                }
            }
        }
        else {
            // going up
            if (cell + delta >= 0) {
                cell += delta;
            }
            else {
                // stop in the first row
                cell = cell % columns;
            }
        }
    }

    moveSelTo(cell);
}

void SimpleGrid::moveSelTo(int cell) {
    if (_cellCount == 0) return;

    cell = std::clamp(cell, 0, static_cast<int>(_cellCount) - 1);
    if (cell != _selectedCell) {
        selectCell(cell);
    }
}

void SimpleGrid::setSelectedCell(int index) {
    if (index < 0 || index >= static_cast<int>(_cellCount)) {
        _selectedCell = -1;
    }
    else {
        _selectedCell = index;
        scrollTo(index);
    }
    _viewport->update();
}

int SimpleGrid::selectedCell() const {
    return _selectedCell;
}

void SimpleGrid::selectCell(int index) {
    _selectedCell = index;
    scrollTo(index);
    Q_EMIT cellSelected(index);
    _viewport->update();
}

void SimpleGrid::openCell(int index) {
    Q_EMIT cellOpened(index);
}

void SimpleGrid::scrollTo(int cell) {
    if (!_layoutValid || !_cellCount) return;

    auto vertScroll = getVScrollPosition();
    auto columns = _colsRows.x();
    auto rows = _colsRows.y();
    auto row = (_flow == Flow::Horizontal)
        ? static_cast<int>(std::floor(cell / columns))
        : (cell % rows);
    auto firstRow = vertScroll / _cellPitch.y();
    auto lastRow = firstRow + _viewportWholeRows;
    auto scroll = vertScroll;
    if (row <= firstRow) {
        // scroll up
        scroll = row * _cellPitch.y();
    }
    else if (row >= lastRow) {
        // scroll down
        scroll = (std::min(row + 1, rows) - _viewportWholeRows) * _cellPitch.y();
        auto max = std::max(0, _areaSize.y() - _scrollArea->height());
        if (scroll > max) scroll = max;
    }

    if (scroll != vertScroll) {
        _scrollArea->verticalScrollBar()->setValue(scroll);
    }
}

void SimpleGrid::drawContent(QPainter* painter, int width, int height) {
    if (!_layoutValid) {
        calcLayout();
    }
    if (!_layoutValid || !_colsRows.x() || !_colsRows.y() || !_cellCount || !_cellPitch.y() || !_cellPitch.x()) return;

    auto vertScroll = getVScrollPosition();

    auto firstRow = vertScroll / _cellPitch.y();
    auto lastRow = std::min(_colsRows.y() - 1, firstRow + _viewportRows);
    const auto columns = _colsRows.x();
    const auto rows = _colsRows.y();
    auto calcCellPos = [this, width, columns](int column) {
        int pitch = _cellPitch.x();
        int x = 0;
        if (_stretchCells) {
            // distribute/stretch cells horizontally across entire width leaving no gaps
            x = column * width / columns;
            auto next = (column + 1) * width / columns;
            pitch = next - x;
        }
        else {
            // cells from left to right with a possible gap at right
            x = column * _cellPitch.x();
        }
        return std::make_pair(x, pitch);
    };

    int maxDrawnRow = firstRow;
    for (int row = firstRow; row <= lastRow; ++row) {
        for (int col = 0; col < columns; ++col) {
            int index = (_flow == Flow::Horizontal) ? (row * columns + col) : (col * rows + row);
            if (index < 0 || index >= static_cast<int>(_cellCount)) continue;

            auto [x, pitch] = calcCellPos(col);
            int y = row * _cellPitch.y();
            auto rect = Geom::IntRect::from_xywh(x, y, pitch - _gap.x(), _cellSize.y());

            if (_drawFunc) {
                _drawFunc(painter, index, rect, _selectable && index == _selectedCell);
            }

            maxDrawnRow = row;
        }
    }

    if (_gap.x() > 0 && _gap.y() > 0 && _paintGaps) {
        QColor fg = palette().color(QPalette::Text);
        fg.setAlphaF(0.15);

        // stay in the center of the gap
        auto center = Geom::IntPoint(_gap.x() + 1, _gap.y() + 1) / 2;

        auto lastRowCols = (_flow == Flow::Horizontal)
            ? static_cast<int>(_cellCount) % columns
            : (_colsRows.y() > 0 ? static_cast<int>(_cellCount) / _colsRows.y() : 0);
        auto limit = lastRowCols != 0 ? std::min(maxDrawnRow + 1, _colsRows.y() - 1) : maxDrawnRow + 1;

        // horizontal lines
        for (int row = firstRow + 1; row <= limit; ++row) {
            auto y = row * _cellPitch.y() - center.y();
            int x = 0;
            painter->fillRect(QRect(x, y, width, 1), fg);
        }

        // vertical lines
        int y = firstRow * _cellPitch.y();
        int bottom = std::min(limit, _colsRows.y()) * _cellPitch.y();
        for (int col = 1; col < _colsRows.x(); ++col) {
            auto [x, pitch] = calcCellPos(col);
            painter->fillRect(QRect(x - center.x(), y, 1, bottom - y), fg);
        }

        if (lastRowCols != 0) {
            // the bottommost row is only partially filled with cells
            y = bottom;
            bottom += _cellPitch.y();
            int right = 0;
            for (int col = 1; col <= lastRowCols; ++col) {
                auto [x, pitch] = calcCellPos(col);
                painter->fillRect(QRect(x - center.x(), y, 1, bottom - y), fg);
                right = x;
            }
            y = bottom;
            int x = 0;
            painter->fillRect(QRect(x, y, width, 1), fg);
        }
    }
}

void SimpleGrid::calcLayout() {
    // calculate layout based on how wide the grid is in the scrolled window
    // and the height of the scrolled window (which is our viewport)
    int width = _scrollArea->viewport()->width();
    int height = _scrollArea->height();
    _layoutValid = calcLayout(width, height);
}

bool SimpleGrid::calcLayout(int width, int height) {
    if (width <= 0 || height <= 0 || _cellSize.x() <= 0 || _cellSize.y() <= 0 || _cellCount == 0) {
        _cellPitch = {};
        _areaSize = {};
        _viewport->setMinimumSize(0, 0);
        return false;
    }

    auto columns = std::max(1, (width + _gap.x()) / (_cellSize.x() + _gap.x()));
    _cellPitch.x() = _stretchCells ? width / columns : _cellSize.x() + _gap.x();
    auto rows = static_cast<int>((_cellCount + columns - 1) / columns); // round up
    _cellPitch.y() = _cellSize.y() + _gap.y();
    _colsRows = {columns, rows};
    _areaSize = { std::max(_cellSize.x(), width), rows * _cellPitch.y() - _gap.y() };
    _viewportRows = std::min(rows, (height + _cellPitch.y() - _gap.y()) / _cellPitch.y());
    _viewportWholeRows = std::clamp((height + _gap.y()) / _cellPitch.y(), 1, rows);

    // Update viewport size so the scrollbar range tracks the new content size.
    // This keeps invalidate() cheap (just marks dirty + queues a repaint) while
    // still ensuring the scrollbar is correct when calcLayout runs during paint.
    _viewport->setMinimumSize(width, _areaSize.y());
    _viewport->setMaximumSize(width, _areaSize.y());
    if (_sizeToContents && _areaSize.y() > 0) {
        // 1px top/bottom margins from the QVBoxLayout
        auto total = _areaSize.y() + 2;
        if (minimumHeight() != total || maximumHeight() != total) {
            setMinimumHeight(total);
            setMaximumHeight(total);
        }
    }

    return true;
}

} // namespace Linea::UI
