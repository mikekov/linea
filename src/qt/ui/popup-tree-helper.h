// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shared keyboard navigation helpers for QTreeWidget-based popups.
 *
 * Used by widgets that show a QTreeWidget inside a PopupMenu with a search
 * box: arrow keys move selection among visible items, Enter activates the
 * current item, Escape closes the popup.
 */

#ifndef LINEA_UI_POPUP_TREE_HELPER_H
#define LINEA_UI_POPUP_TREE_HELPER_H

#include <QKeyEvent>
#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <vector>

namespace Linea::UI {

/// Collect all visible items in depth-first order. Unlike a "leaves only"
/// variant, this includes branch nodes — suitable for trees where every
/// node is selectable (e.g. layers).
inline void collectVisibleItems(QTreeWidgetItem* parent,
                                std::vector<QTreeWidgetItem*>& items) {
    for (int i = 0; i < parent->childCount(); ++i) {
        auto child = parent->child(i);
        if (child->isHidden()) continue;
        items.push_back(child);
        if (child->childCount() > 0) {
            collectVisibleItems(child, items);
        }
    }
}

/// Collect visible leaf items only (nodes with no children).
inline void collectVisibleLeaves(QTreeWidgetItem* parent,
                                 std::vector<QTreeWidgetItem*>& leaves) {
    for (int i = 0; i < parent->childCount(); ++i) {
        auto child = parent->child(i);
        if (child->isHidden()) continue;
        if (child->childCount() == 0) {
            leaves.push_back(child);
        } else {
            collectVisibleLeaves(child, leaves);
        }
    }
}

/// Find the first visible leaf in depth-first order, or nullptr.
inline QTreeWidgetItem* firstVisibleLeaf(QTreeWidget* tree) {
    if (!tree) return nullptr;
    std::vector<QTreeWidgetItem*> leaves;
    collectVisibleLeaves(tree->invisibleRootItem(), leaves);
    return leaves.empty() ? nullptr : leaves.front();
}

/// Find the first visible item (leaf or branch) in depth-first order, or nullptr.
inline QTreeWidgetItem* firstVisibleItem(QTreeWidget* tree) {
    if (!tree) return nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto item = tree->topLevelItem(i);
        if (!item->isHidden()) return item;
    }
    return nullptr;
}

/**
 * Handle a key event for popup tree navigation. Returns true if the event
 * was consumed (caller should stop processing it).
 *
 * @param keyEvent  The key event from an eventFilter.
 * @param tree      The QTreeWidget to navigate.
 * @param popup     The popup widget to close on Escape/Enter.
 * @param onActivate Called when Enter is pressed on a valid current item.
 * @param leavesOnly If true, navigate only among leaf nodes; if false,
 *                   navigate among all visible nodes.
 */
template <typename ActivateFn>
bool handlePopupTreeKey(QKeyEvent* keyEvent, QTreeWidget* tree, QWidget* popup,
                        ActivateFn onActivate, bool leavesOnly = true) {
    int key = keyEvent->key();

    if (key == Qt::Key_Up || key == Qt::Key_Down) {
        std::vector<QTreeWidgetItem*> items;
        if (leavesOnly) {
            collectVisibleLeaves(tree->invisibleRootItem(), items);
        } else {
            collectVisibleItems(tree->invisibleRootItem(), items);
        }
        if (items.empty()) return true;

        int dir = (key == Qt::Key_Down) ? 1 : -1;
        auto current = tree->currentItem();
        int idx = -1;
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i] == current) { idx = static_cast<int>(i); break; }
        }
        int next = idx + dir;
        if (next < 0) next = 0;
        if (next >= static_cast<int>(items.size())) next = static_cast<int>(items.size()) - 1;

        tree->setCurrentItem(items[next]);
        tree->scrollToItem(items[next]);
        return true;
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        auto item = tree->currentItem();
        if (item && !item->isHidden() && (!leavesOnly || item->childCount() == 0)) {
            onActivate(item);
        }
        return true;
    }

    if (key == Qt::Key_Escape) {
        popup->hide();
        return true;
    }

    return false;
}

} // namespace Linea::UI

#endif // LINEA_UI_POPUP_TREE_HELPER_H
