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

#include "simple-list.h"

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
#include <QStyle>

#include <algorithm>

namespace Linea::UI {

SimpleList::SimpleList(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    construct();
}

SimpleList::~SimpleList() {
    // _viewport is owned by the scroll area, which will delete it
}

void SimpleList::construct() {
    setObjectName("SimpleList");
    setProperty("class", "SimpleList");
    setProperty("frame", _hasFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setFrameShape(QFrame::NoFrame);

    _viewport = new SimpleListViewport(this);
    _viewport->setProperty("class", "active-background");
    setViewport(_viewport);

    setFocusPolicy(Qt::StrongFocus);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        _viewport->update();
    });
}

SimpleList::SimpleListViewport::SimpleListViewport(SimpleList* parent)
    : QWidget(parent)
    , _list(parent)
{
    setMouseTracking(true);
}

void SimpleList::SimpleListViewport::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    _list->drawContent(&painter, width(), height());
}

bool SimpleList::SimpleListViewport::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        auto row = _list->getRowIndex(helpEvent->pos().x(), helpEvent->pos().y());
        if (row >= 0 && _list->_tooltipFunc) {
            QString text = _list->_tooltipFunc(row);
            if (!text.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos(), text, this);
                return true;
            }
        }
    }
    return QWidget::event(event);
}

void SimpleList::setAverageRowHeight(int height) {
    height = std::max(0, height);
    if (_averageRowHeight != height) {
        _averageRowHeight = height;
        invalidate();
    }
}

int SimpleList::averageRowHeight() const {
    return _averageRowHeight;
}

void SimpleList::setRowGap(int gap) {
    gap = std::max(0, gap);
    if (_gap != gap) {
        _gap = gap;
        invalidate();
    }
}

int SimpleList::rowGap() const {
    return _gap;
}

void SimpleList::setShowGap(bool show) {
    if (_paintGaps != show) {
        _paintGaps = show;
        _viewport->update();
    }
}

void SimpleList::setSelectable(bool isSelectable) {
    // todo - readonly
    Q_UNUSED(isSelectable);
}

void SimpleList::setHasFrame(bool frame) {
    _hasFrame = frame;
    setProperty("frame", frame);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void SimpleList::setRowCount(std::size_t count) {
    _selectedRow = -1;
    _visibleRows.clear();
    if (count == 0) {
        verticalScrollBar()->setValue(0);
    }

    if (_rowCount != count) {
        _rowCount = count;
        invalidate();
    }
}

std::size_t SimpleList::rowCount() const {
    return _rowCount;
}

void SimpleList::invalidate() {
    _layoutValid = false;
    // Trigger layout recalculation on next paint or resize
    _viewport->update();
}

void SimpleList::clear() {
    setRowCount(0);
    _viewport->update();
}

void SimpleList::setDrawFunc(DrawFunc func) {
    _drawFunc = std::move(func);
    invalidate();
}

void SimpleList::setTooltipFunc(TooltipFunc func) {
    _tooltipFunc = std::move(func);
}

void SimpleList::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    resizeLayout();
}

void SimpleList::showEvent(QShowEvent* event) {
    QAbstractScrollArea::showEvent(event);
    // Force layout recalculation when first shown to ensure correct row count
    resizeLayout();
}

void SimpleList::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Map coordinates from SimpleList to the viewport
        auto viewportPos = viewport()->mapFrom(this, event->position().toPoint());
        _clickedRow = getRowIndex(viewportPos.x(), viewportPos.y());
        setFocus();
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void SimpleList::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        auto viewportPos = viewport()->mapFrom(this, event->position().toPoint());
        auto index = getRowIndex(viewportPos.x(), viewportPos.y());
        if (index >= 0 && index == _clickedRow && _selectedRow != index) {
            selectRow(index);
        }
        _clickedRow = -1;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void SimpleList::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        auto viewportPos = viewport()->mapFrom(this, event->position().toPoint());
        auto index = getRowIndex(viewportPos.x(), viewportPos.y());
        if (index >= 0) {
            openRow(index);
        }
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void SimpleList::resizeLayout() {
    // Calculate layout based on the viewport size.
    int width = viewport()->width();
    int height = viewport()->height();
    _layoutValid = calcLayout(width, height);
    _viewport->update();
}

int SimpleList::getRowIndex(double x, double y) const {
    if (_visibleRows.empty()) return -1;

    double width = _viewport->width();
    double height = _viewport->height();
    if (x < 0 || y < 0 || x >= width || y >= height) return -1;

    for (const auto& row : _visibleRows) {
        if (y >= row.y && y < row.y + row.height) {
            return row.index;
        }
    }

    return -1;
}

int SimpleList::getVScrollPosition() const {
    // The scroll bar value is a row index, i.e. a percentage of the total rows.
    return verticalScrollBar()->value();
}

void SimpleList::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Up:
        moveSel(-1);
        break;
    case Qt::Key_Down:
        moveSel(1);
        break;
    case Qt::Key_PageUp:
        moveSel(-std::max(1, pageStepRows() - 1));
        break;
    case Qt::Key_PageDown:
        moveSel(std::max(1, pageStepRows() - 1));
        break;
    case Qt::Key_Home:
        moveSelTo(0);
        break;
    case Qt::Key_End:
        moveSelTo(_rowCount - 1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (_selectedRow >= 0) {
            openRow(_selectedRow);
        }
        break;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    event->accept();
}

void SimpleList::moveSel(int delta) {
    if (_rowCount == 0 || !_layoutValid) return;

    int row = _selectedRow >= 0 ? _selectedRow : 0;
    row += delta;
    moveSelTo(row);
}

void SimpleList::moveSelTo(int row) {
    if (_rowCount == 0) return;

    row = std::clamp(row, 0, static_cast<int>(_rowCount) - 1);
    if (row != _selectedRow) {
        selectRow(row);
    }
}

void SimpleList::setSelectedRow(int index) {
    if (index < 0 || index >= static_cast<int>(_rowCount)) {
        _selectedRow = -1;
    }
    else {
        _selectedRow = index;
        scrollTo(index);
    }
    _viewport->update();
}

int SimpleList::selectedRow() const {
    return _selectedRow;
}

void SimpleList::selectRow(int index) {
    _selectedRow = index;
    scrollTo(index);
    Q_EMIT rowSelected(index);
    _viewport->update();
}

void SimpleList::openRow(int index) {
    Q_EMIT rowOpened(index);
}

void SimpleList::scrollTo(int row) {
    if (_rowCount == 0 || row < 0 || row >= static_cast<int>(_rowCount)) return;

    if (!_visibleRows.empty()) {
        int first = _visibleRows.front().index;
        int last = _visibleRows.back().index;
        if (row >= first && row <= last) {
            return;
        }
    }

    auto maxScroll = std::max(0, static_cast<int>(_rowCount) - 1);
    verticalScrollBar()->setValue(std::clamp(row, 0, maxScroll));
}

int SimpleList::drawRow(QPainter* painter, int index, int y, int width, bool topDown) {
    if (!_drawFunc) return 0;
    int h = _drawFunc(painter, index, y, width, index == _selectedRow, topDown);
    return std::max(0, h);
}

void SimpleList::drawContent(QPainter* painter, int width, int height) {
    if (!_layoutValid) {
        _layoutValid = calcLayout(width, height);
    }
    if (!_layoutValid || _rowCount == 0) return;

    int rows = static_cast<int>(_rowCount);
    _visibleRows.clear();

    int pivotRow = getVScrollPosition();
    pivotRow = std::clamp(pivotRow, 0, rows - 1);
    bool topDown = rows <= 1 || pivotRow < rows / 2;

    QColor gapColor;
    int gapCenter = 0;
    if (_paintGaps && _gap > 0) {
        gapColor = palette().color(QPalette::Text);
        gapColor.setAlphaF(0.15);
        gapCenter = (_gap + 1) / 2;
    }

    if (topDown) {
        int y = 0;
        for (int index = pivotRow; index < rows && y < height; ++index) {
            int h = drawRow(painter, index, y, width, true);
            if (h > 0) {
                _visibleRows.push_back({index, h, y});
            }
            if (_paintGaps && _gap > 0 && index + 1 < rows) {
                int lineY = y + h + _gap - gapCenter;
                if (lineY >= 0 && lineY < height) {
                    painter->fillRect(QRect(0, lineY, width, 1), gapColor);
                }
            }
            y += h + _gap;
        }
    }
    else {
        int bottom = height;
        for (int index = pivotRow; index >= 0 && bottom > 0; --index) {
            int h = drawRow(painter, index, bottom, width, false);
            if (h > 0) {
                int y = bottom - h;
                _visibleRows.push_back({index, h, y});
            }
            if (_paintGaps && _gap > 0 && index > 0) {
                int lineY = bottom - h - gapCenter;
                if (lineY >= 0 && lineY < height) {
                    painter->fillRect(QRect(0, lineY, width, 1), gapColor);
                }
            }
            bottom -= h + _gap;
        }
    }

    std::sort(_visibleRows.begin(), _visibleRows.end(),
        [](const VisibleRow& a, const VisibleRow& b) { return a.index < b.index; });

    _pageStepRows = std::max(1, static_cast<int>(_visibleRows.size()));
    verticalScrollBar()->setPageStep(_pageStepRows);
}

bool SimpleList::calcLayout(int width, int height) {
    if (width <= 0 || height <= 0 || _rowCount == 0) {
        _pageStepRows = 1;
        return false;
    }

    int rows = static_cast<int>(_rowCount);

    // Scroll bar position is a row index, independent of the actual row heights.
    auto vbar = verticalScrollBar();
    int maxValue = std::max(0, rows - 1);
    vbar->setRange(0, maxValue);
    vbar->setSingleStep(1);

    // Use the last observed page step if we have one, otherwise fall back to
    // an estimate based on the supplied average row height.
    int pageStep = _pageStepRows;
    if (pageStep <= 1 && _averageRowHeight > 0) {
        pageStep = std::max(1, (height + _averageRowHeight - 1) / _averageRowHeight);
    }
    vbar->setPageStep(std::max(1, pageStep));

    return true;
}

int SimpleList::pageStepRows() const {
    return std::max(1, _pageStepRows);
}

} // namespace Linea::UI
