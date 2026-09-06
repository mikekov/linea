// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * VirtualTreeList — virtual tree widget for variable height rows.
 */
/*
 * Authors:
 *   Michael Kowalski
 */

#include "virtual-tree-list.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QStyle>
#include <QWheelEvent>

namespace Linea::UI {

VirtualTreeList::VirtualTreeList(QWidget* parent) : QWidget(parent) {

    setObjectName("VirtualTreeList");
    setProperty("class", "VirtualTreeList");
    setProperty("frame", true);
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // timer used to allow accumulation of the user's typed input for filtering
    _typingTimer = new QTimer(this);
    _typingTimer->setSingleShot(true);
    _typingTimer->setInterval(1000); // 1 second timeout
    connect(_typingTimer, &QTimer::timeout, this, [this]() {
        // after a pause in typing, clear the buffer so user can start a new search
        _alphanumericBuffer.clear();
    });

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _scrollBar = new QScrollBar(Qt::Vertical, this);
    _scrollBar->setRange(0, 0);
    _scrollBar->setSingleStep(1);
    _scrollBar->setPageStep(10);

    connect(_scrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        setFirstVisibleItem(offsetToItemIndex(value));
        // scrollToItem(offsetToItemIndex(value));
        _layoutValid = false;
        updateLayout();
        update(); // Redraw when scrollbar changes
    });

    layout->addStretch(1); // Content area
    layout->addWidget(_scrollBar);
}

VirtualTreeList::~VirtualTreeList() = default;

void VirtualTreeList::setItemCount(int count) {
    _topItemCount = count;
    _totalItemCount = count;
    _expandedRows.clear();
    _firstVisibleItem = ItemIndex(0);
    invalidateLayout();
    updateLayout();
    updateScrollBar();
    _scrollBar->setValue(calcScrollOffset(_firstVisibleItem));
    update();

}

void VirtualTreeList::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    int m = _hasFrame ? 1 : 0;
    QMargins margins(m, m, m, m);
    drawContent(&painter, rect().marginsRemoved(margins));
}

void VirtualTreeList::wheelEvent(QWheelEvent* event) {
    _pendingWheelDelta += event->angleDelta().y();

    // we scroll by row, not pixel, so slow it down
    constexpr int wheelStep = 40;
    int steps = _pendingWheelDelta / wheelStep;
    if (steps != 0) {
        _pendingWheelDelta -= steps * wheelStep;
        _scrollBar->setValue(_scrollBar->value() - steps);
    }
    event->accept();
}

void VirtualTreeList::setGetSubitemCountFunc(GetSubitemCountFunc func) {
    _getSubitemCountFunc = func;
    invalidateLayout();
}

int VirtualTreeList::subitemCount(int topLevelItemIndex) const{
    return _getSubitemCountFunc ? _getSubitemCountFunc(topLevelItemIndex) : 0;
}

std::optional<VirtualTreeList::ItemIndex> VirtualTreeList::nextRow(const VirtualTreeList::ItemIndex& current) const {
    if (current.isSubitem()) {
        int subCount = subitemCount(current.parentIndex);
        if (current.itemIndex + 1 < subCount) {
            return ItemIndex(current.itemIndex + 1, current.parentIndex);
        } else if (current.parentIndex + 1 < _topItemCount) {
            return ItemIndex(current.parentIndex + 1);
        }
    } else {
        if (isItemExpanded(current.itemIndex)) {
            int subCount = subitemCount(current.itemIndex);
            if (subCount > 0) {
                return ItemIndex(0, current.itemIndex);
            }
        }
        if (current.itemIndex + 1 < _topItemCount) {
            return ItemIndex(current.itemIndex + 1);
        }
    }
    return std::nullopt;
}

std::optional<VirtualTreeList::ItemIndex> VirtualTreeList::prevRow(const VirtualTreeList::ItemIndex& current) const {
    if (current.isSubitem()) {
        if (current.itemIndex > 0) {
            return ItemIndex(current.itemIndex - 1, current.parentIndex);
        } else {
            return ItemIndex(current.parentIndex);
        }
    } else {
        if (current.itemIndex > 0) {
            int prevTop = current.itemIndex - 1;
            if (isItemExpanded(prevTop)) {
                int subCount = subitemCount(prevTop);
                if (subCount > 0) {
                    return ItemIndex(subCount - 1, prevTop);
                }
            }
            return ItemIndex(prevTop);
        }
    }
    return std::nullopt;
}

std::optional<VirtualTreeList::ItemIndex> VirtualTreeList::lastRow() const {
    if (!_topItemCount) return std::nullopt;

    // Start from the last possible item
    ItemIndex lastItem = ItemIndex(_topItemCount - 1);

    // If the last top-level item is expanded, go to its last subitem
    if (_expandedRows.contains(_topItemCount - 1)) {
        int subCount = subitemCount(_topItemCount - 1);
        if (subCount > 0) {
            lastItem = ItemIndex(subCount - 1, _topItemCount - 1);
        }
    }

    // Now walk backwards, adding heights until we fill the viewport with full rows
    int viewportHeight = height();
    int accumulatedHeight = rowHeight(lastItem);
    std::optional<ItemIndex> current = lastItem;

    while (true) {
        auto prev = prevRow(*current);
        if (!prev) break;

        int h = rowHeight(*prev);
        accumulatedHeight += h + _rowGap;
        if (accumulatedHeight > viewportHeight) {
            break;
        }

        current = prev;
    }

    return current;
}

void VirtualTreeList::setGetRowHeightFunc(GetRowHeightFunc func) {
    _getRowHeightFunc = func;
    update();
}

void VirtualTreeList::setDrawItemFunc(DrawItemFunc func) {
    _drawItemFunc = func;
    update();
}

int VirtualTreeList::rowHeight(const ItemIndex& index) const {
    if (!_getRowHeightFunc) return 0;

    // State state;
    // if (auto it = _rowStates.find(index); it != _rowStates.end()) {
    //     state = it->second;
    //     if (state.height > 0) {
    //         return state.height;
    //     }
    // }

    int height = _getRowHeightFunc(index);
    // state.height = height;
    // _rowStates[index] = state;
    return height;
}

std::optional<VirtualTreeList::ItemIndex> VirtualTreeList::getPageRows(const ItemIndex& start, bool down) const {
    if (!_topItemCount) return std::nullopt;

    int viewportHeight = height();
    int accumulatedHeight = 0;

    std::optional<ItemIndex> current = start;
    while (current.has_value()) {
        int h = rowHeight(*current);
        accumulatedHeight += h + _rowGap;
        if (accumulatedHeight > viewportHeight) {
            break;
        }
        auto next = down ? nextRow(*current) : prevRow(*current);
        if (!next) break;

        current = next;
    }

    return current;
}

void VirtualTreeList::setItemExpanded(int parentIndex, bool expand) {
    ItemIndex index(parentIndex);

    bool is_expanded = _expandedRows.contains(parentIndex);
    if (expand == is_expanded) {
        return;
    }

    if (expand) {
        _expandedRows.insert(parentIndex);
        _totalItemCount += subitemCount(parentIndex);
    } else {
        _expandedRows.erase(parentIndex);
        _totalItemCount -= subitemCount(parentIndex);

        // fewer rows available, verify we still fill the viewport
        setFirstVisibleItem(_firstVisibleItem);
    }

    updateLayout();
    update();
}

bool VirtualTreeList::isItemExpanded(int parentIndex) const {
    return _expandedRows.contains(parentIndex);
}

void VirtualTreeList::setRowGap(int gap) {
    _rowGap = gap;
    update();
}

void VirtualTreeList::setExpanderEnabled(bool enabled) {
    _expanderEnabled = enabled;
    update();
}

void VirtualTreeList::setHoverEnabled(bool enabled) {
    _hoverEnabled = enabled;
    if (!enabled) {
        _hoveredItem = ItemIndex();
        update();
    }
}

void VirtualTreeList::setHasFrame(bool frame) {
    if (_hasFrame == frame) return;
    _hasFrame = frame;
    setProperty("frame", frame);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void VirtualTreeList::setHasActiveBackground(bool active) {
    if (_hasActiveBackground == active) return;
    _hasActiveBackground = active;
    setProperty("active-bgnd", active);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void VirtualTreeList::invalidate() {
    invalidateLayout();
    update();
}

void VirtualTreeList::invalidateLayout() {
    _layoutValid = false;
    update();
}

VirtualTreeList::ItemIndex VirtualTreeList::selectedItem() const {
    return _selectedItem;
}

void VirtualTreeList::setSelectedItem(const ItemIndex& index) {
    selectItem(index);
}

void VirtualTreeList::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateLayout();
    updateScrollBar();
}

void VirtualTreeList::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateLayout();
    updateScrollBar();
}

void VirtualTreeList::keyPressEvent(QKeyEvent* event) {
    if (!_topItemCount) {
        QWidget::keyPressEvent(event);
        return;
    }

    // Check for alphanumeric input
    QString text = event->text();
    if (!text.isEmpty() && (text[0].isLetterOrNumber() || text[0].isSpace())) {
        _alphanumericBuffer += text;
        _typingTimer->start();
        Q_EMIT alphanumericInput(_alphanumericBuffer);
        // Don't consume the event, let clients handle it
        QWidget::keyPressEvent(event);
        return;
    }

    // Clear buffer on any other keyboard input
    if (!_alphanumericBuffer.isEmpty()) {
        _alphanumericBuffer.clear();
        _typingTimer->stop();
    }

    switch (event->key()) {
        case Qt::Key_Up:
            moveSelection(-1);
            event->accept();
            break;
        case Qt::Key_Down:
            moveSelection(1);
            event->accept();
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (_selectedItem.itemIndex >= 0 || _selectedItem.isSubitem()) {
                Q_EMIT itemActivated(_selectedItem);
            }
            event->accept();
            break;
        case Qt::Key_Left:
            if (_selectedItem.isSubitem() && isItemExpanded(_selectedItem.parentIndex)) {
                setItemExpanded(_selectedItem.parentIndex, false);
                selectItem(ItemIndex(_selectedItem.parentIndex));
            }
            else if (_selectedItem.isTopLevel() && isItemExpanded(_selectedItem.itemIndex)) {
                setItemExpanded(_selectedItem.itemIndex, false);
                selectItem(ItemIndex(_selectedItem.itemIndex));
            }
            event->accept();
            break;
        case Qt::Key_Right:
            if (!_selectedItem.isSubitem() && subitemCount(_selectedItem.itemIndex) > 0) {
                setItemExpanded(_selectedItem.itemIndex, true);
            }
            event->accept();
            break;

        case Qt::Key_Space:
            if (!_selectedItem.isSubitem()) {
                toggleExpansion(_selectedItem);
            }
            event->accept();
            break;
        case Qt::Key_PageUp:
            // Move up by one page
            if (auto prev = getPageRows(_selectedItem, false)) {
                moveSelectionAbsolute(*prev);
            }
            event->accept();
            break;
        case Qt::Key_PageDown:
            // Move down by one page
            if (auto next = getPageRows(_selectedItem, true)) {
                moveSelectionAbsolute(*next);
            }
            event->accept();
            break;
        case Qt::Key_Home:
            // Jump to first item
            moveSelectionAbsolute(ItemIndex(0));
            event->accept();
            break;
        case Qt::Key_End:
            // Jump to last item
            moveSelectionAbsolute(ItemIndex(_topItemCount - 1));
            event->accept();
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void VirtualTreeList::mousePressEvent(QMouseEvent* event) {
    if (!_topItemCount) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        ItemIndex clickedItem = getItemAtPosition(event->position().y());

        if (clickedItem.itemIndex >= 0 || clickedItem.isSubitem() && _expanderEnabled) {
            // Check if click is on expand/collapse indicator (left side)
            // int indent = clickedItem.isSubitem() ? _indent + _expanderWidth : _indent;
            if (event->position().x() < _expanderWidth && !clickedItem.isSubitem()) {
                _pressedExpander = clickedItem;
            } else {
                selectItem(clickedItem);
            }
        }

        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void VirtualTreeList::mouseReleaseEvent(QMouseEvent* event) {
    if (!_topItemCount) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && _pressedExpander.itemIndex >= 0) {
        ItemIndex releasedItem = getItemAtPosition(event->position().y());

        // Only toggle if released over the same expander
        if (releasedItem == _pressedExpander && event->position().x() < _expanderWidth) {
            toggleExpansion(_pressedExpander);
        }

        _pressedExpander = ItemIndex();
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void VirtualTreeList::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!_topItemCount) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        ItemIndex clickedItem = getItemAtPosition(event->position().y());

        if (clickedItem.itemIndex >= 0 || clickedItem.isSubitem()) {
            Q_EMIT itemActivated(clickedItem);
        }

        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void VirtualTreeList::mouseMoveEvent(QMouseEvent* event) {
    if (!_hoverEnabled || !_topItemCount) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    ItemIndex hovered = getItemAtPosition(event->position().y());
    bool needsUpdate = hovered != _hoveredItem;

    if (needsUpdate) {
        _hoveredItem = hovered;
    }

    // Check if mouse is over an expander (full clickable area, not just the triangle)
    ItemIndex expanderHover;
    if (!hovered.isSubitem() && subitemCount(hovered.itemIndex) > 0 && event->position().x() < _expanderWidth) {
        expanderHover = hovered;
    }

    if (expanderHover != _hoveredExpander) {
        _hoveredExpander = expanderHover;
        needsUpdate = true;
    }

    if (needsUpdate) {
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void VirtualTreeList::leaveEvent(QEvent* event) {
    if (_hoverEnabled) {
        _hoveredItem = ItemIndex();
        _hoveredExpander = ItemIndex();
        update();
    }
    QWidget::leaveEvent(event);
}

void VirtualTreeList::updateLayout() {
    if (!_topItemCount) return;

    // build layout for visible rows

    _layout.clear();
    int viewportHeight = height();
    int y = 0;
    ItemIndex index = _firstVisibleItem;

    while (true) {
        int h = rowHeight(index);
        _layout.push_back({index, y, h});
        y += h + _rowGap;

        auto next = nextRow(index);
        if (!next || y > viewportHeight) break;
        index = *next;
    }

    _layoutValid = true;
}

void VirtualTreeList::updateScrollBar() {
    if (!_topItemCount) return;

    int pageStep = 0;
    int viewportHeight = height();
    int accumulatedHeight = 0;

    auto current = _firstVisibleItem;
    while (true) {
        int h = rowHeight(current);
        if (accumulatedHeight + h > viewportHeight) break;
        accumulatedHeight += h + _rowGap;
        ++pageStep;

        auto next = nextRow(current);
        if (!next) break;
        current = *next;
    }
    int max = 0;
    if (auto lastPossible = lastRow()) {
        max = calcScrollOffset(*lastPossible);
    }

    _scrollBar->setRange(0, max);
    _scrollBar->setPageStep(std::max(1, pageStep));
}

void VirtualTreeList::drawRow(QPainter* painter, const ItemIndex& index, const QRect& rowRect, bool selected, bool hovered) {
    // Draw selection background
    if (selected) {
        painter->fillRect(rowRect, palette().highlight());
    } else if (hovered) {
        auto brush = palette().highlight();
        auto color = brush.color();
        color.setAlphaF(color.alphaF() * 0.30);
        brush.setColor(color);
        painter->fillRect(rowRect, brush);
    }

    int indent = index.isSubitem() ? _indent : 0;
    if (_expanderEnabled) {
        indent += _expanderWidth;
    }
    int w = rowRect.width() - indent;
    if (w <= 0) return;

    // Draw the item
    _drawItemFunc(painter, index, rowRect.adjusted(indent, 0, 0, 0), selected);

    // Draw row separator
    if (_rowGap > 0 && _rowSeparatorsEnabled) {
        QRect rect(rowRect.left(), rowRect.y() + rowRect.height(), rowRect.width(), _rowGap);
        painter->fillRect(rect, palette().midlight());
    }
}

void VirtualTreeList::drawExpander(QPainter* painter, const QRect& rect, bool expanded, bool hovered) const {
    if (hovered) {
        auto brush = palette().mid();
        auto color = brush.color();
        color.setAlphaF(color.alphaF() * 0.60);
        brush.setColor(color);
        painter->fillRect(rect, brush);
    }
    int size = 16;
    int iconX = rect.x() + (rect.width() - size) / 2 + 1;
    int iconY = rect.y() + (rect.height() - size) / 2;
    QIcon::Mode mode = hovered ? QIcon::Active : QIcon::Normal;
    QIcon::State state = expanded ? QIcon::On : QIcon::Off;
    const QIcon& icon = expanded ? _expandedIcon : _collapsedIcon;
    icon.paint(painter, iconX, iconY, size, size, Qt::AlignCenter, mode, state);
}

void VirtualTreeList::drawContent(QPainter* painter, const QRect& rect) {
    // Draw background
    painter->fillRect(rect, palette().base());

    if (!_layoutValid) {
        updateLayout();
    }

    for (auto& row : _layout) {
        bool hovered = _hoveredItem == row.index;
        bool selected = _selectedItem == row.index;
        QRect rowRect(rect.left(), row.yPosition, rect.width(), row.height);
        drawRow(painter, row.index, rowRect, selected, hovered);

        if (!row.index.isSubitem() && subitemCount(row.index.itemIndex) > 0 && _expanderEnabled) {
            // Draw expander
            QRect expanderRect(0, row.yPosition, _expanderWidth, row.height);
            bool expanderHovered = _hoveredExpander == row.index;
            drawExpander(painter, expanderRect, isItemExpanded(row.index.itemIndex), expanderHovered);
        }
    }
    return;
}

VirtualTreeList::ItemIndex VirtualTreeList::getItemAtPosition(int yPos) const {
    if (!_layoutValid) return ItemIndex();

    for (const auto& row : _layout) {
        if (yPos >= row.yPosition && yPos < row.yPosition + row.height) {
            return row.index;
        }
    }
    return ItemIndex(); // Not found
}

void VirtualTreeList::selectItem(const ItemIndex& index) {
    if (index.itemIndex == _selectedItem.itemIndex &&
        index.isSubitem() == _selectedItem.isSubitem() &&
        index.parentIndex == _selectedItem.parentIndex) {
        return;
    }
// printf("selected: %d, %d\n", index.itemIndex, index.parentIndex);
    _selectedItem = index;
    update();
    Q_EMIT selectionChanged(index);

    scrollToItem(index);
}

void VirtualTreeList::moveSelection(int delta) {
    if (!_topItemCount) return;

    auto newIndex = delta > 0 ? nextRow(_selectedItem) : prevRow(_selectedItem);
    if (newIndex.has_value()) {
        selectItem(newIndex.value());
    }
}

void VirtualTreeList::moveSelectionAbsolute(const ItemIndex& index) {
    if (!_topItemCount) return;

    selectItem(index);
    // scrollToItem(index);
}

void VirtualTreeList::toggleExpansion(const ItemIndex& index) {
    if (index.isSubitem()) {
        return;
    }

    bool currentExpanded = isItemExpanded(index.itemIndex);
    setItemExpanded(index.itemIndex, !currentExpanded);
    update();
    updateLayout();
    updateScrollBar();
}

int VirtualTreeList::firstVisibleRow() const {
    if (!_topItemCount) return -1;

    auto index = _scrollBar->value();
    return index;
}

void VirtualTreeList::setFirstVisibleItem(const ItemIndex& index) {
    // Scroll up - check if we would overshoot
    auto lastPossible = lastRow();
// printf("last: %d %d\n", lastPossible.value().itemIndex, lastPossible.value().parentIndex);
        // _firstVisibleItem = index;
        // return;

    if (lastPossible && index > *lastPossible) {
// printf("limit hit; attempted: %d %d\n", index.itemIndex, index.parentIndex);
        _firstVisibleItem = *lastPossible;
    } else {
        _firstVisibleItem = index;
    }

    updateScrollBar();
}

void VirtualTreeList::scrollToItem(const ItemIndex& index) {
    if (!_topItemCount) return;

    if (index.isSubitem()) {
        // make sure that subitem is visible (i.e. parent is expanded)
        setItemExpanded(index.parentIndex, true);
    }

    // Ensure layout is valid
    if (!_layoutValid) {
        updateLayout();

        if (_layout.empty()) return;
    }

    if (index < _firstVisibleItem) {
        // Scroll up
        setFirstVisibleItem(index);
    }
    else {
        // Scroll down
        int viewportHeight = height();
        const auto& row = _layout.back();
        // find last fully visible row
        const auto& last = row.yPosition + row.height <= viewportHeight || _layout.size() <= 1 ? row.index : _layout[_layout.size() - 2].index;
// printf("last: %d\n", last.itemIndex);
// printf("back: %d, y %d,  h %d\n", row.index.itemIndex, row.yPosition + row.height, viewportHeight);

        if (index <= last) {
            // Item is already visible, no need to scroll
// printf("visible: %d\n", index.itemIndex);
            return;
        }

        // we need to scroll: calculate height from destination row "index" upward
        // until we approach the viewport height
        int accumulatedHeight = rowHeight(index);
        auto current = index;

        while (true) {
            auto prev = prevRow(current);
            if (!prev) break;

            int h = rowHeight(*prev);
            if (accumulatedHeight + h + _rowGap > viewportHeight) break;

            accumulatedHeight += h + _rowGap;
            current = *prev;
        }

        setFirstVisibleItem(current);
// printf("top: %d\n", _firstVisibleItem.itemIndex);
    }

    updateLayout();
    _scrollBar->setValue(calcScrollOffset(_firstVisibleItem));
    update();
}

VirtualTreeList::ItemIndex VirtualTreeList::offsetToItemIndex(int offset) const {
    if (!_topItemCount || offset < 0) return ItemIndex(0);

    int currentOffset = offset;

    // Walk through expanded rows in order
    for (int expandedIndex : _expandedRows) {
        if (expandedIndex > currentOffset) {
            break; // This expanded row is beyond our target
        }

        int subCount = subitemCount(expandedIndex);
        if (currentOffset == expandedIndex) {
            // Offset points to the expanded top-level item itself
            return ItemIndex(expandedIndex);
        }

        if (currentOffset > expandedIndex) {
            // Check if offset falls within this expanded row's subitems
            if (currentOffset < expandedIndex + 1 + subCount) {
                // Offset is within subitems
                int subitemIndex = currentOffset - expandedIndex - 1;
                return ItemIndex(subitemIndex, expandedIndex);
            }
            // Subtract subitems to continue searching
            currentOffset -= subCount;
        }
    }

    // If we get here, it's a top-level item
    return ItemIndex(currentOffset);
}

int VirtualTreeList::calcScrollOffset(const ItemIndex& index) const {
    if (!_topItemCount) return 0;

    int offset = 0;
    int limit = index.isTopLevel() ? index.itemIndex : index.parentIndex;

    for (int expandedIndex : _expandedRows) {
        if (expandedIndex < limit) {
            offset += subitemCount(expandedIndex);
        }
    }

    if (index.isTopLevel()) {
        offset += index.itemIndex;
    } else {
        offset += index.parentIndex + 1 + index.itemIndex;
    }
    return offset;
}

void VirtualTreeList::setRowSeparatorsEnabled(bool enabled) {
    _rowSeparatorsEnabled = enabled;
    update();
}

} // namespace Linea::UI
