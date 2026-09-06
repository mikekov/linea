// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * VirtualTreeList — virtual tree widget for variable height rows.
 */
/*
 * Authors:
 *   Michael Kowalski
 */

#ifndef LINEA_UI_VIRTUAL_TREE_LIST_H
#define LINEA_UI_VIRTUAL_TREE_LIST_H

#include <cstdint>
#include <QWidget>
#include <QScrollBar>
#include <QIcon>
#include <QTimer>
#include <functional>
#include <cstddef>
#include <set>
#include <optional>

namespace Linea::UI {

/**
 * Virtual tree widget that efficiently handles thousands of items with variable height rows.
 * Only renders items visible in the viewport. Does not own data - uses callbacks to query.
 * Supports single-level depth (top-level items with optional subitems).
 */
class VirtualTreeList : public QWidget {
    Q_OBJECT

public:
    explicit VirtualTreeList(QWidget* parent = nullptr);
    ~VirtualTreeList() override;

    // Index identifying an item in the tree structure.
    // Can refer to either a top-level item or a subitem.
    struct ItemIndex {
        int itemIndex;      // Index within its level (top-level or subitems)
        int parentIndex;    // For subitems: parent's top-level index

        ItemIndex() : itemIndex(-1), parentIndex(-1) {}
        ItemIndex(int itemIdx, std::size_t parentIdx = -1)
            : itemIndex(itemIdx), parentIndex(parentIdx) {}

        bool isSubitem() const { return parentIndex >= 0; }
        bool isTopLevel() const { return parentIndex < 0; }
        bool isValid() const { return itemIndex >= 0; }

        bool operator == (const ItemIndex& other) const {
            return itemIndex == other.itemIndex &&
                parentIndex == other.parentIndex;
        }

        bool operator < (const ItemIndex& other) const {
            if (isSubitem() == other.isSubitem()) {
                if (isSubitem()) {
                    if (parentIndex != other.parentIndex) {
                        return parentIndex < other.parentIndex;
                    }
                    return itemIndex < other.itemIndex;
                }
                return itemIndex < other.itemIndex;
            }
            if (isSubitem()) {
                // this is a subitem, other is top-level
                return parentIndex < other.itemIndex;
            }
            // this is top-level, other is a subitem
            if (itemIndex == other.parentIndex) {
                return true; // top-level items come before their subitems
            }
            return itemIndex < other.parentIndex;
        }

        bool operator > (const ItemIndex& other) const {
            return !(*this < other) && !(*this == other);
        }

        bool operator <= (const ItemIndex& other) const {
            return *this < other || *this == other;
        }
    };

    // set number of top level items
    void setItemCount(int count);

    // Callback to get number of subitems for a given top level item
    using GetSubitemCountFunc = std::function<int (int topLevelItemIndex)>;
    void setGetSubitemCountFunc(GetSubitemCountFunc func);

    using GetRowHeightFunc = std::function<int (const ItemIndex& index)>;
    void setGetRowHeightFunc(GetRowHeightFunc func);

    // Callback to draw an item
    using DrawItemFunc = std::function<void (QPainter* painter, const ItemIndex& index, const QRect& rect, bool selected)>;
    void setDrawItemFunc(DrawItemFunc func);

    // Expand/collapse a parent item
    void setItemExpanded(int parentIndex, bool expand);
    bool isItemExpanded(int parentIndex) const;

    // Enable/disable drawing row separators
    void setRowSeparatorsEnabled(bool enabled);
    void setRowGap(int gap);

    void setExpanderEnabled(bool enabled);

    // Enable/disable hover tracking
    void setHoverEnabled(bool enabled);

    // Add or remove a frame around the tree list
    void setHasFrame(bool frame = true);
    void setHasActiveBackground(bool active = true);

    // Invalidate cached measurements and redraw
    void invalidate();

    // Get currently selected item index
    ItemIndex selectedItem() const;
    // Set selected item by index
    void setSelectedItem(const ItemIndex& index);

Q_SIGNALS:
    // Emitted when selection changes
    void selectionChanged(const ItemIndex& index);
    // Emitted when an item is double-clicked
    void itemActivated(const ItemIndex& index);
    // Emitted when alphanumeric key is pressed
    void alphanumericInput(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void construct();
    void updateLayout();
    int subitemCount(int topLevelItemIndex) const;
    std::optional<ItemIndex> nextRow(const ItemIndex& current) const;
    std::optional<ItemIndex> prevRow(const ItemIndex& current) const;
    std::optional<ItemIndex> lastRow() const;
    std::optional<ItemIndex> getPageRows(const ItemIndex& start, bool down) const;
    void drawContent(QPainter* painter, const QRect& rect);
    void drawExpander(QPainter* painter, const QRect& rect, bool expanded, bool hovered) const;
    ItemIndex getItemAtPosition(int y) const;
    void selectItem(const ItemIndex& index);
    void moveSelection(int delta); // relative movement
    void moveSelectionAbsolute(const ItemIndex& index); // absolute movement
    void toggleExpansion(const ItemIndex& index);
    void scrollToItem(const ItemIndex& index);
    void invalidateLayout();
    void updateScrollBar();
    double rowPosition() const { return _scrollBar->value(); }
    int rowHeight(const ItemIndex& index) const;
    int firstVisibleRow() const;
    void drawRow(QPainter* painter, const ItemIndex& index, const QRect& rect, bool selected, bool hovered);
    int calcScrollOffset(const ItemIndex& index) const;
    ItemIndex offsetToItemIndex(int offset) const;
    void setFirstVisibleItem(const ItemIndex& index);

    // Callbacks
    GetSubitemCountFunc _getSubitemCountFunc;
    DrawItemFunc _drawItemFunc;
    GetRowHeightFunc _getRowHeightFunc;
    // State
    int _topItemCount = 0;
    int _totalItemCount = 0;
    ItemIndex _firstVisibleItem;
    ItemIndex _selectedItem;
    ItemIndex _hoveredItem;
    ItemIndex _hoveredExpander; // tracks which row's expander is hovered
    ItemIndex _pressedExpander; // tracks expander pressed for release toggle
    int _rowGap = 1;
    int _indent = 15;
    int _expanderWidth = 16;
    bool _rowSeparatorsEnabled = true;
    bool _hoverEnabled = true;
    bool _hasFrame = true;
    bool _hasActiveBackground = true;
    QString _alphanumericBuffer;
    QTimer* _typingTimer;
    QIcon _expandedIcon = QIcon(":/icons/pan-down");
    QIcon _collapsedIcon = QIcon(":/icons/pan-end");
    // expanded top row indexes (sorted!)
    mutable std::set<int> _expandedRows;
    struct RowInfo {
        ItemIndex index;
        int yPosition = 0;
        int height = 0;

        bool operator == (const RowInfo& other) const {
            return index == other.index &&
                yPosition == other.yPosition &&
                height == other.height;
        }
    };

    std::vector<RowInfo> _layout;
    bool _layoutValid = false;
    bool _expanderEnabled = true;

    QScrollBar* _scrollBar;
    int _pendingWheelDelta = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_VIRTUAL_TREE_LIST_H
