// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * TabStrip — Generic strip-of-tabs widget
 */
/*
 * Authors:
 *   PBS <pbs3141@gmail.com>
 *   Mike Kowalski
 *
 * Copyright (C) 2024-2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_TAB_STRIP_H
#define LINEA_UI_TAB_STRIP_H

#include <QWidget>
#include <memory>
#include <optional>
#include <vector>

QT_BEGIN_NAMESPACE
class QMenu;
class QToolButton;
class QMimeData;
QT_END_NAMESPACE

namespace Linea::UI {

struct TabItem;
class TabDragState;

/**
 * A custom tab strip widget that displays a horizontal (or vertical) row of tabs.
 *
 * Each tab has an icon, an optional text label, and an optional close button.
 * Tabs can be reordered by dragging within the strip (Internally) or dragged
 * out to other TabStrip instances (Externally).  A (+) new-tab button can be
 * shown at the trailing edge when a popup menu is attached.
 *
 * The widget does its own layout: tabs share the available width proportionally
 * using the same shrink/expand algorithm as the GTK original.
 */
class TabStrip : public QWidget {
    Q_OBJECT
    Q_PROPERTY(Alignment alignment READ alignment WRITE setAlignment)

public:
    /// Controls whether tabs can be reordered by dragging.
    enum class Rearrange {
        Never,      ///< Drag-reordering is disabled entirely.
        Internally, ///< Tabs can only be moved within this strip.
        Externally, ///< Tabs can also be dragged to other strips (or detached).
    };

    /// Controls when tab text labels are shown.
    enum class ShowLabels {
        Never,      ///< Labels are never shown; only the icon is visible.
        Always,     ///< Labels are always shown.
        ActiveOnly, ///< Only the active tab shows its label.
    };

    /// Controls how the tab row is positioned in the strip.
    enum class Alignment {
        Left,
        Center,
        Right,
    };
    Q_ENUM(Alignment)

    explicit TabStrip(QWidget* parent = nullptr);
    explicit TabStrip(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~TabStrip() override;

    // ── Tab management ───────────────────────────────────────────────────────

    /// Add a new tab at @p pos (-1 = append).  Returns the opaque tab handle.
    /// The returned pointer is valid until the tab is removed; do not delete it.
    QWidget* addTab(const QString& label, const QString& icon, int pos = -1);

    /// Remove the tab whose handle is @p tab.
    void removeTab(const QWidget& tab);
    void removeTabAt(int pos);

    /// Mark @p tab as the active tab (deselecting any previously active tab).
    void selectTab(const QWidget& tab);
    void selectTabAt(int pos);

    /// Reorder the internal tab list to match @p sorted (a permutation of get_tabs()).
    void setTabsOrder(std::vector<QWidget*> sorted);

    /// Return the tab handles in display order.
    std::vector<QWidget*> getTabs() const;

    /// Return the 0-based position of @p tab, or -1 if not found.
    int getTabPosition(const QWidget& tab) const;

    /// Return the tab handle at position @p i, or nullptr.
    QWidget* getTabAt(int i) const;

    void setTabLabel(const QWidget& tab, const QString& label);
    void setTabIcon(const QWidget& tab, const QString& icon);
    void setTabTooltip(const QWidget& tab, const QString& tooltip);

    // ── Appearance ────────────────────────────────────────────────────────────

    /// Attach a popup menu to the (+) new-tab button (pass nullptr to hide it).
    void setNewTabPopup(QMenu* menu);

    /// Attach a context menu that appears on right-click over any tab.
    void setTabsContextPopup(QMenu* menu);

    void setRearrangingTabs(Rearrange mode);
    void setShowLabels(ShowLabels mode);
    void setShowCloseButton(bool show);

    /// Show a drag-handle icon on the trailing edge of every tab.
    void setDrawHandle(bool show = true);

    void setAlignment(Alignment align);
    Alignment alignment() const { return _alignment; }

    // ── Queries ───────────────────────────────────────────────────────────────

    bool isTabActive(const QWidget& tab) const;
    Qt::Orientation tabOrientation() const { return _orientation; }

    // ── Drag-and-drop helpers ─────────────────────────────────────────────────

    /// MIME type string used as the drag payload when a tab is dragged.
    static const char* dndMimeType();

    /// Attempt to unpack a drop's MIME data into (source TabStrip*, tab index).
    static std::optional<std::pair<TabStrip*, int>> unpack_drop_source(const QMimeData* data);

Q_SIGNALS:
    /// The user clicked (or middle-clicked activating) a tab — caller should
    /// call selectTab() in response.
    void tabSelectRequested(QWidget* tab);

    /// The user clicked the close button or middle-clicked a tab.
    void tabCloseRequested(QWidget* tab);

    /// The user dragged a tab off every known strip — should be turned into a
    /// floating window by the caller.
    void tabFloatRequested(QWidget* tab);

    /// A tab was dragged from @p srcStrip (at @p srcIndex) and dropped onto
    /// this strip at @p dstIndex.
    void tabMoveRequested(QWidget* tab, int srcIndex, TabStrip* srcStrip, int dstIndex);

    /// A tab was reordered within this strip from @p fromIndex to @p toIndex.
    void tabRearranged(int fromIndex, int toIndex);

    /// Emitted when the user starts dragging a tab outside the strip bounds.
    void dndBegin();

    /// Emitted when a drag-and-drop operation ends.
    /// @p cancelled is true when the drag was aborted without a drop.
    void dndEnd(bool cancelled);

protected:
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool event(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    // Layout helpers
    void _doLayout();
    void _updatePlusButton();
    int _plusButtonWidth() const;

    // Hit-testing
    TabItem* _tabAtPoint(QPoint pos) const;
    TabItem* _findTab(const QWidget& widget) const;

    // Reordering
    bool _reorderTab(int from, int to);

    // Drag state management
    void _startDrag(TabItem* tab, QPoint pressOffset);
    void _updateDragDrop(QPoint pos);
    void _finishDrag(bool cancel = false);

    Qt::Orientation _orientation;

    std::vector<std::shared_ptr<TabItem>> _tabs;
    TabItem* _activeTab = nullptr;

    // (+) new-tab button at the trailing edge
    QToolButton* _plusBtn = nullptr;
    QMenu* _newTabMenu = nullptr;

    // Right-click context menu
    QMenu* _contextMenu = nullptr;

    Rearrange _rearrange = Rearrange::Externally;
    ShowLabels _showLabels = ShowLabels::Never;
    bool _showCloseBtn = true;
    bool _showDragHandle = false;
    Alignment _alignment = Alignment::Left;

    // Left-button press tracking for drag initiation
    TabItem* _pressedTab = nullptr;
    QPoint _pressPos;
    bool _pressMoved = false;

    // Active drag session (non-null while a drag is in progress)
    std::unique_ptr<TabDragState> _drag;

    // Tab whose close button is currently hovered (nullptr = none)
    TabItem* _hoveredCloseTab = nullptr;

    static constexpr int MARGIN = 4;

    friend class TabDragState;
};

} // namespace Linea::UI

#endif // LINEA_UI_TAB_STRIP_H
