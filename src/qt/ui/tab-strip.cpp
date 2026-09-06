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

#include "tab-strip.h"

#include <QApplication>
#include <QDrag>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>
#include <QHelpEvent>
#include <QToolButton>
#include <QToolTip>
#include <algorithm>
#include <cassert>

namespace Linea::UI {

// ── Internal constants ────────────────────────────────────────────────────────

static constexpr int DRAG_INIT_DIST  = 8;   // px before a press becomes a drag
static constexpr int DETACH_DIST     = 25;  // px outside strip before tab detaches
// Tab width bounds (horizontal orientation)
static constexpr int TAB_MIN_WIDTH   = 60;  // never shrink below this
static constexpr int TAB_MAX_WIDTH   = 200; // never grow beyond this

// MIME type that identifies an in-app tab drag payload
static const char* const kTabMimeType = "application/x-linea-tab-strip";

// ── Size helpers (mirrors the GTK shrink/expand algorithm) ────────────────────

namespace {

struct Size {
    int minimum = 0;
    int delta = 0; // natural - minimum
    bool expand = false;
    int index = 0;
    int size() const { return minimum + delta; }
};

/// Reduce total size by @p decrease, taking from the widest items first.
void shrink_sizes(std::vector<Size>& sizes, int decrease) {
    if (sizes.empty() || decrease <= 0) return;

    std::sort(sizes.begin(), sizes.end(), [](const Size& a, const Size& b) { return a.delta > b.delta; });

    int available = 0;
    for (auto& s : sizes)
        available += s.delta;
    decrease = std::min(available, decrease);

    // sentinel
    sizes.push_back(Size{0, 0, false, 99999});

    auto entry = &sizes.front();
    while (decrease > 0) {
        entry->delta--;
        decrease--;
        if (decrease == 0) break;
        if (entry[1].delta > entry->delta) {
            ++entry;
        } else {
            entry = &sizes.front();
        }
    }
    sizes.pop_back();

    std::sort(sizes.begin(), sizes.end(), [](const Size& a, const Size& b) { return a.index < b.index; });
}

/// Grow total size by @p increase, distributing evenly to expanding items first.
void expand_sizes(std::vector<Size>& sizes, int increase) {
    if (sizes.empty() || increase <= 0) return;

    std::sort(sizes.begin(), sizes.end(), [](const Size& a, const Size& b) {
        if (a.expand != b.expand) return a.expand > b.expand;
        return a.size() < b.size();
    });

    // sentinel
    sizes.push_back(Size{0, 0, false, 99999});

    while (increase > 0) {
        int i = 0;
        while (sizes[i].size() == sizes[i + 1].size() && sizes[i + 1].expand)
            i++;

        int to_inc = increase / (i + 1);
        if (sizes[i + 1].expand) to_inc = std::min(to_inc, sizes[i + 1].size() - sizes[i].size());

        for (int j = 0; j <= i; j++) {
            sizes[j].delta += to_inc;
            increase -= to_inc;
        }
        if (!sizes[i + 1].expand) {
            for (int j = 0; j <= i && increase > 0; j++) {
                sizes[j].delta++;
                increase--;
            }
        }
    }
    sizes.pop_back();

    std::sort(sizes.begin(), sizes.end(), [](const Size& a, const Size& b) { return a.index < b.index; });
}

} // namespace

// ── TabItem ───────────────────────────────────────────────────────────────────

/**
 * Internal structure holding per-tab state and cached geometry.
 *
 * Each TabItem doubles as the opaque QWidget* handle returned from addTab().
 * It is a QWidget child of the TabStrip so that it can receive events and be
 * painted; all actual painting is delegated back to TabStrip::paintEvent().
 */
struct TabItem : public QWidget {
    explicit TabItem(TabStrip* parent)
        : QWidget(parent)
        , strip(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    TabStrip* strip = nullptr;

    QString label;
    QString tooltip;
    QIcon icon;
    bool active = false;

    // Geometry assigned by _doLayout
    QRect rect;

    // Sub-rects (in TabStrip coordinates) computed during layout
    QRect iconRect;
    QRect labelRect;
    QRect closeRect;
    QRect handleRect;
    bool iconVisible = false;
    bool labelVisible = false;
    bool closeVisible = false;
    bool handleVisible = false;
};

/// Draw a tab's label text with appropriate horizontal alignment.
/// Centers the label when there is no icon and the text fits @p rect without
/// clipping; otherwise left-aligns (so a fade-out gradient on overflow reads
/// naturally from the left). The caller is responsible for clipping and any
/// fade-out masking.
void drawTabLabel(QPainter& p, const TabItem& t, const QRect& rect,
                  const QColor& pen, bool singleLine) {
    if (!t.labelVisible || t.labelRect.isEmpty()) return;

    p.setPen(pen);
    const int labelW = p.fontMetrics().horizontalAdvance(t.label);
    const auto halign = (!t.iconVisible && labelW <= rect.width()) ? Qt::AlignHCenter : Qt::AlignLeft;
    const auto flags = Qt::AlignVCenter | halign | (singleLine ? Qt::TextSingleLine : Qt::Alignment());
    p.drawText(rect, flags, t.label);
}

// ── TabDragState ──────────────────────────────────────────────────────────────

/**
 * Encapsulates the state of an ongoing tab drag operation.
 *
 * Created when the user has dragged far enough from the press point.
 * Destroyed (via unique_ptr reset) when the drag finishes.
 */
class TabDragState {
public:
    TabDragState(TabItem* src, QPoint pressOffset, TabStrip* strip)
        : _src(src)
        , _pressOffset(pressOffset)
        , _srcStrip(strip)
        , _dstStrip(strip) {}

    TabItem* src() const { return _src; }
    TabStrip* srcStrip() const { return _srcStrip; }
    TabStrip* dstStrip() const { return _dstStrip; }

    /// Current drop index within dstStrip (if a valid drop position is known).
    std::optional<int> dropIndex() const { return _dropIndex; }

    /// Current drag position in dstStrip coordinates (if the pointer is over dstStrip).
    std::optional<int> dropPos() const { return _dropPos; }

    /// Ghost position: where the tab floats visually (strip coordinates, top-left of ghost rect).
    std::optional<QPoint> ghostPos() const { return _ghostPos; }

    QPoint pressOffset() const { return _pressOffset; }

    void setDstStrip(TabStrip* dst) { _dstStrip = dst; }
    void setDropPos(std::optional<int> pos) { _dropPos = pos; }
    void setDropIndex(int i) { _dropIndex = i; }
    void setGhostPos(std::optional<QPoint> pos) { _ghostPos = pos; }

private:
    TabItem* _src;
    QPoint _pressOffset;
    TabStrip* _srcStrip;
    TabStrip* _dstStrip;

    std::optional<int> _dropPos;
    std::optional<int> _dropIndex;
    std::optional<QPoint> _ghostPos;
};

// ── TabStrip ──────────────────────────────────────────────────────────────────

TabStrip::TabStrip(QWidget* parent)
    : TabStrip(Qt::Horizontal, parent) {
}

TabStrip::TabStrip(Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent)
    , _orientation(orientation) {
    setObjectName("TabStrip");
    setAttribute(Qt::WA_StyledBackground, true);

    // Accept drops for cross-strip tab migration
    setAcceptDrops(true);
    // Track mouse at all times (for hover highlighting, not just button held down)
    setMouseTracking(true);

    // (+) new-tab button — hidden by default until a menu is attached
    _plusBtn = new QToolButton(this);
    _plusBtn->setObjectName("NewTabButton");
    _plusBtn->setIcon(QIcon(":/icons/list-add"));
    _plusBtn->setIconSize(QSize(16, 16));
    _plusBtn->setAutoRaise(true);
    _plusBtn->setFocusPolicy(Qt::NoFocus);
    _plusBtn->hide();

    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy::Expanding : QSizePolicy::Preferred,
                  orientation == Qt::Horizontal ? QSizePolicy::Fixed : QSizePolicy::Expanding);
}

TabStrip::~TabStrip() {
    // Cancel any in-progress drag cleanly
    if (_drag) {
        _finishDrag(true);
    }
}

// ── dndMimeType / unpack_drop_source ─────────────────────────────────────────

const char* TabStrip::dndMimeType() {
    return kTabMimeType;
}

std::optional<std::pair<TabStrip*, int>> TabStrip::unpack_drop_source(const QMimeData* data) {
    if (!data || !data->hasFormat(kTabMimeType)) return {};

    QByteArray encoded = data->data(kTabMimeType);
    if (encoded.size() != static_cast<int>(sizeof(quintptr) + sizeof(int))) return {};

    quintptr stripPtr;
    int tabIndex;
    memcpy(&stripPtr, encoded.constData(), sizeof(quintptr));
    memcpy(&tabIndex, encoded.constData() + sizeof(quintptr), sizeof(int));

    return std::make_pair(reinterpret_cast<TabStrip*>(stripPtr), tabIndex);
}

// ── Tab management ────────────────────────────────────────────────────────────

QWidget* TabStrip::addTab(const QString& label, const QString& icon, int pos) {
    auto item = std::make_shared<TabItem>(this);
    item->label = label;
    item->icon = icon.isEmpty() ? QIcon() : QIcon(":/icons/" + icon);
    item->iconVisible = !item->icon.isNull();
    item->labelVisible = (_showLabels == ShowLabels::Always);
    item->closeVisible = false;
    item->handleVisible = _showDragHandle;

    if (pos < 0 || pos > static_cast<int>(_tabs.size())) pos = static_cast<int>(_tabs.size());

    _tabs.insert(_tabs.begin() + pos, item);

    _doLayout();
    update();
    return item.get();
}

void TabStrip::removeTab(const QWidget& tab) {
    int i = getTabPosition(tab);
    if (i < 0) return;

    if (_activeTab == _tabs[i].get()) _activeTab = nullptr;
    if (_pressedTab == _tabs[i].get()) _pressedTab = nullptr;
    if (_hoveredCloseTab == _tabs[i].get()) _hoveredCloseTab = nullptr;

    if (_drag && _drag->src() == _tabs[i].get()) {
        _finishDrag(true);
    }

    _tabs.erase(_tabs.begin() + i);
    _doLayout();
    update();
}

void TabStrip::removeTabAt(int pos) {
    if (auto* t = getTabAt(pos)) removeTab(*t);
}

void TabStrip::selectTab(const QWidget& tab) {
    TabItem* found = _findTab(tab);
    if (!found || found == _activeTab) return;

    if (_activeTab) {
        _activeTab->active = false;
    }
    _activeTab = found;
    _activeTab->active = true;

    // Refresh label/close visibility
    for (auto& t : _tabs) {
        t->closeVisible = _showCloseBtn && t->active;
        t->labelVisible = (_showLabels == ShowLabels::Always) || (_showLabels == ShowLabels::ActiveOnly && t->active);
    }

    _doLayout();
    update();
}

void TabStrip::selectTabAt(int pos) {
    if (auto* t = getTabAt(pos)) selectTab(*t);
}

void TabStrip::setTabLabel(const QWidget& tab, const QString& label) {
    TabItem* found = _findTab(tab);
    if (!found) return;
    found->label = label;
    _doLayout();
    update();
}

void TabStrip::setTabTooltip(const QWidget& tab, const QString& tooltip) {
    TabItem* found = _findTab(tab);
    if (!found) return;
    found->tooltip = tooltip;
}

void TabStrip::setTabIcon(const QWidget& tab, const QString& icon) {
    TabItem* found = _findTab(tab);
    if (!found) return;
    found->icon = icon.isEmpty() ? QIcon() : QIcon(":/icons/" + icon);
    found->iconVisible = !found->icon.isNull();
    _doLayout();
    update();
}

void TabStrip::setTabsOrder(std::vector<QWidget*> sorted) {
    std::sort(_tabs.begin(), _tabs.end(), [&sorted](const auto& a, const auto& b) {
        auto ia = std::find(sorted.begin(), sorted.end(), a.get());
        auto ib = std::find(sorted.begin(), sorted.end(), b.get());
        return ia < ib;
    });
    _doLayout();
    update();
}

std::vector<QWidget*> TabStrip::getTabs() const {
    std::vector<QWidget*> out;
    out.reserve(_tabs.size());
    for (auto& t : _tabs)
        out.push_back(t.get());
    return out;
}

int TabStrip::getTabPosition(const QWidget& tab) const {
    for (int i = 0; i < static_cast<int>(_tabs.size()); i++) {
        if (_tabs[i].get() == &tab) return i;
    }
    return -1;
}

QWidget* TabStrip::getTabAt(int i) const {
    auto idx = static_cast<std::size_t>(i);
    return idx < _tabs.size() ? _tabs[idx].get() : nullptr;
}

// ── Appearance setters ────────────────────────────────────────────────────────

void TabStrip::setNewTabPopup(QMenu* menu) {
    _newTabMenu = menu;
    _plusBtn->setMenu(menu);
    _updatePlusButton();
}

void TabStrip::setTabsContextPopup(QMenu* menu) {
    _contextMenu = menu;
}

void TabStrip::setRearrangingTabs(Rearrange mode) {
    _rearrange = mode;
}

void TabStrip::setShowLabels(ShowLabels mode) {
    _showLabels = mode;
    for (auto& t : _tabs) {
        t->labelVisible = (mode == ShowLabels::Always) || (mode == ShowLabels::ActiveOnly && t->active);
    }
    _doLayout();
    update();
}

void TabStrip::setShowCloseButton(bool show) {
    _showCloseBtn = show;
    for (auto& t : _tabs) {
        t->closeVisible = show && t->active;
    }
    _doLayout();
    update();
}

void TabStrip::setDrawHandle(bool show) {
    _showDragHandle = show;
    for (auto& t : _tabs) {
        t->handleVisible = show;
    }
    _doLayout();
    update();
}

void TabStrip::setAlignment(Alignment align) {
    if (_alignment == align) return;

    _alignment = align;
    _doLayout();
}

bool TabStrip::isTabActive(const QWidget& tab) const {
    return _activeTab && _activeTab == &tab;
}

// ── Private: layout ───────────────────────────────────────────────────────────

int TabStrip::_plusButtonWidth() const {
    if (!_plusBtn->isVisible()) return 0;
    return _plusBtn->sizeHint().width();
}

/**
 * Compute the geometry of every tab and the (+) button.
 *
 * Mirrors the GTK size_allocate_vfunc logic: tabs share the strip width
 * proportionally, shrinking from the widest-first when space is tight, or
 * growing expanding tabs when there is extra space.
 */
void TabStrip::_doLayout() {
    const bool horiz = (_orientation == Qt::Horizontal);
    const int W = width();
    const int H = height();

    const int plusW = _plusButtonWidth();
    // Available space for tabs
    int available = horiz ? (W - plusW) : (H - plusW);

    // Per-tab natural and minimum sizes along the layout axis
    const QFontMetrics fm(font());
    const int closeW = fm.height() + 4 + MARGIN; // close button width (matches sub-rect layout)
    constexpr int iconSpace = 16;
    auto tabNatSize = [&](const TabItem& t) -> int {
        if (!horiz) return t.labelVisible ? 80 : 32;
        int nat = 2 * MARGIN; // left + right outer padding (sub-rect uses r.x()+MARGIN and w-=2*MARGIN)
        if (t.iconVisible)   nat += iconSpace;      // iconW (sub-rect only advances x by iconW)
        if (t.closeVisible)  nat += closeW;  // cw + MARGIN
        if (t.labelVisible)  nat += fm.horizontalAdvance(t.label) + 2 * MARGIN; // text + MARGIN left + MARGIN right of text
        if (t.handleVisible) nat += 16 + MARGIN; // hw + MARGIN
        return std::min(nat, TAB_MAX_WIDTH);
    };
    auto tabMinSize = [&](const TabItem& t) -> int {
        if (!horiz) return 32;
        int minS = 2 * MARGIN; // base outer padding
        if (t.iconVisible) minS += iconSpace;
        // Close button and handle are mandatory — protect them from shrinking.
        // Only label space can be reclaimed by shrink_sizes.
        if (t.closeVisible) minS += closeW;     // cw + MARGIN
        if (t.handleVisible) minS += 16 + MARGIN; // hw + MARGIN
        return t.labelVisible ? std::max(minS, TAB_MIN_WIDTH) : minS;
    };

    // The dragged tab is hidden from the layout — it renders as a floating ghost instead
    auto isDragSrc = [&](const TabItem& t) -> bool { return _drag && _drag->src() == &t; };

    // Gather sizes
    std::vector<Size> alloc;
    alloc.reserve(_tabs.size());
    int totalNat = 0, totalMin = 0;
    bool hasExpanding = false;
    for (int i = 0; i < static_cast<int>(_tabs.size()); i++) {
        auto& t = *_tabs[i];
        if (isDragSrc(t)) {
            alloc.push_back(Size{0, 0, false, i});
            continue;
        }
        int mn = tabMinSize(t);
        int nat = tabNatSize(t);
        nat = std::max(nat, mn);
        totalNat += nat;
        totalMin += mn;
        bool exp = false; // tabs don't expand beyond their natural (capped) size
        hasExpanding = hasExpanding || exp;
        alloc.push_back(Size{mn, nat - mn, exp, i});
    }

    // Adjust sizes to fit available space
    if (available <= totalMin) {
        for (auto& a : alloc) {
            a.delta = 0;
        }
    } else if (available < totalNat) {
        shrink_sizes(alloc, totalNat - available);
    } else if (hasExpanding) {
        expand_sizes(alloc, available - totalNat);
    }

    // Reserve a slot for a drag-drop insertion indicator
    std::optional<int> dropPos;
    std::optional<int> dropSize;
    if (_drag && _drag->dstStrip() == this && _drag->dropPos()) {
        int sz = 0;
        int srcIdx = getTabPosition(*_drag->src());
        if (srcIdx >= 0) {
            sz = alloc[srcIdx].size();
            if (sz == 0) {
                // src is hidden; use its natural size
                sz = tabNatSize(*_drag->src());
            }
        } else {
            // External tab: estimate from natural size
            sz = tabNatSize(*_drag->src());
        }
        int limit = available - sz;
        dropPos = limit > 0 ? std::clamp(*_drag->dropPos(), 0, limit) : 0;
        dropSize = sz;
    }

    // Sum final tab widths so the whole row (tabs + plus) can be aligned
    std::vector<int> widths;
    widths.reserve(_tabs.size());
    int totalTabLen = 0;
    for (int i = 0; i < static_cast<int>(_tabs.size()); i++) {
        if (isDragSrc(*_tabs[i])) {
            widths.push_back(0);
            continue;
        }
        int sz = alloc[i].size();
        widths.push_back(sz);
        totalTabLen += sz;
    }

    const int layoutLen = horiz ? W : H;
    const int groupLen = totalTabLen + plusW;
    int start = 0;
    switch (_alignment) {
        case Alignment::Center:
            start = std::max(0, (layoutLen - groupLen) / 2);
            break;
        case Alignment::Right:
            start = std::max(0, layoutLen - groupLen);
            break;
        case Alignment::Left:
        default:
            break;
    }

    // Assign rects
    int pos = start;
    bool dropDone = false;
    for (int i = 0; i < static_cast<int>(_tabs.size()); i++) {
        auto& t = *_tabs[i];
        if (isDragSrc(t)) continue;

        const int sz = widths[i];

        // Insert drop gap before this tab if the dragged tab belongs here
        if (dropPos && !dropDone && pos + sz / 2 > *dropPos) {
            if (_drag) _drag->setDropIndex(i);
            pos += *dropSize;
            dropDone = true;
        }

        QRect r = horiz ? QRect(pos, 0, sz, H) : QRect(0, pos, W, sz);
        t.rect = r;
        pos += sz;
    }

    // Drop at end
    if (dropPos && !dropDone) {
        if (_drag) _drag->setDropIndex(static_cast<int>(_tabs.size()));
    }

    // Position (+) button immediately after the last tab
    if (_plusBtn->isVisible()) {
        if (horiz)
            _plusBtn->setGeometry(pos, 0, plusW, H);
        else
            _plusBtn->setGeometry(0, pos, W, plusW);
    }

    // Compute per-tab sub-rects
    for (auto& tabPtr : _tabs) {
        auto& t = *tabPtr;
        if (isDragSrc(t)) continue;

        const QRect& r = t.rect;
        QFontMetrics fm(font());
        int x = r.x() + MARGIN;
        int y = r.y();
        int w = r.width() - 2 * MARGIN;
        int h = r.height();

        // Handle on the right
        if (t.handleVisible) {
            int hw = 16;
            t.handleRect = QRect(r.right() - hw - MARGIN, y, hw, h);
            w -= hw + MARGIN;
        } else {
            t.handleRect = QRect();
        }

        // Icon (only if one was provided)
        const int iconW = 16;
        if (t.iconVisible) {
            t.iconRect = QRect(x, y + (h - iconW) / 2, iconW, iconW);
            x += iconW;
            w -= iconW;
        } else {
            t.iconRect = QRect();
        }

        // Close button (right side, before handle)
        if (t.closeVisible) {
            int cw = fm.height() + 4;
            int cx = t.handleVisible ? t.handleRect.left() - cw - MARGIN : r.right() - cw - MARGIN;
            t.closeRect = QRect(cx, y + (h - cw) / 2, cw, cw);
            w -= cw + MARGIN;
        } else {
            t.closeRect = QRect();
        }

        // Label — always inset by MARGIN on both sides for consistent breathing room
        if (t.labelVisible && w > 2 * MARGIN) {
            t.labelRect = QRect(x + MARGIN, y, w - 2 * MARGIN, h);
        } else {
            t.labelRect = QRect();
        }

        // If no label and there is an icon, center the icon horizontally
        if (t.labelRect.isEmpty() && t.iconVisible) {
            int cx = r.x() + (r.width() - (t.closeVisible ? t.closeRect.width() + MARGIN : 0) - iconW) / 2;
            t.iconRect.moveLeft(cx);
        }
    }

    update();
}

void TabStrip::_updatePlusButton() {
    bool show = _newTabMenu && (_orientation == Qt::Horizontal);
    _plusBtn->setVisible(show);
    _doLayout();
}

// ── Private: hit-testing ──────────────────────────────────────────────────────

TabItem* TabStrip::_tabAtPoint(QPoint pos) const {
    for (auto it = _tabs.rbegin(); it != _tabs.rend(); ++it) {
        auto& t = **it;
        if (t.rect.contains(pos)) return &t;
    }
    return nullptr;
}

TabItem* TabStrip::_findTab(const QWidget& widget) const {
    for (auto& t : _tabs) {
        if (t.get() == &widget) return t.get();
    }
    return nullptr;
}

// ── Private: reordering ───────────────────────────────────────────────────────

bool TabStrip::_reorderTab(int from, int to) {
    const int n = static_cast<int>(_tabs.size());
    if (from < 0 || from >= n) return false;
    if (to < 0 || to > n) return false;
    if (from == to || from + 1 == to) return false;

    auto tab = std::move(_tabs[from]);
    _tabs.erase(_tabs.begin() + from);
    _tabs.insert(_tabs.begin() + to - (to > from ? 1 : 0), std::move(tab));
    return true;
}

// ── Private: drag management ──────────────────────────────────────────────────

void TabStrip::_startDrag(TabItem* tab, QPoint pressOffset) {
    _drag = std::make_unique<TabDragState>(tab, pressOffset, this);

    // Build the MIME payload: pointer to this strip + tab index
    int idx = getTabPosition(*tab);
    QByteArray encoded(sizeof(quintptr) + sizeof(int), '\0');
    quintptr ptrVal = reinterpret_cast<quintptr>(this);
    memcpy(encoded.data(), &ptrVal, sizeof(quintptr));
    memcpy(encoded.data() + sizeof(quintptr), &idx, sizeof(int));

    auto* mime = new QMimeData;
    mime->setData(kTabMimeType, encoded);

    // Render a pixmap of the tab for the drag cursor
    QPixmap dragPixmap(tab->rect.size());
    dragPixmap.fill(Qt::transparent);
    {
        QPainter p(&dragPixmap);
        p.setOpacity(0.85);
        // Fill with the active tab background palette colour
        QStyleOption opt;
        opt.initFrom(this);
        opt.rect = QRect(QPoint(0, 0), tab->rect.size());
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

        // Draw icon
        if (!tab->icon.isNull()) {
            QRect ir = tab->iconRect.translated(-tab->rect.topLeft());
            tab->icon.paint(&p, ir);
        }
        // Draw label
        drawTabLabel(p, *tab, tab->labelRect.translated(-tab->rect.topLeft()),
                     p.pen().color(), false);
    }

    auto qdrag = new QDrag(this);
    qdrag->setMimeData(mime);
    qdrag->setPixmap(dragPixmap);
    qdrag->setHotSpot(pressOffset);

    Q_EMIT dndBegin(); // re-emit: live drag was cancelled before escalating to QDrag

    // exec() runs a nested event loop; the rest of finish happens after it returns
    Qt::DropAction result = qdrag->exec(Qt::MoveAction);

    // If the drag was not accepted by any drop target, treat it as a float request
    bool cancelled = (result == Qt::IgnoreAction);

    if (_drag) { // may have been cleared by dropEvent already
        _finishDrag(cancelled);
    }
}

void TabStrip::_finishDrag(bool cancel) {
    if (!_drag) return;

    // Capture needed state before reset
    const std::optional<int> dropIdx = _drag->dropIndex();
    TabItem* src = _drag->src();

    _drag.reset();
    _doLayout(); // restore source tab into layout before any paint

    Q_EMIT dndEnd(cancel);

    if (cancel || !dropIdx) {
        update();
        return;
    }

    // Apply the reorder
    const int from = getTabPosition(*src);
    if (from < 0) return;
    if (_reorderTab(from, *dropIdx)) {
        const int realTo = *dropIdx - (*dropIdx > from ? 1 : 0);
        _doLayout(); // recompute rects after order change
        update();
        Q_EMIT tabRearranged(from, realTo);
    }
}

void TabStrip::_updateDragDrop(QPoint pos) {
    if (!_drag) return;

    const bool horiz = (_orientation == Qt::Horizontal);
    const int W = width(), H = height();
    const int plusW = _plusButtonWidth();
    const int stripLen = horiz ? (W - plusW) : (H - plusW);

    // Get the dragged tab's size (for gap reservation and ghost sizing)
    int srcIdx = getTabPosition(*_drag->src());
    int tabSz = 16 + 2 * MARGIN; // fallback
    if (srcIdx >= 0 && srcIdx < static_cast<int>(_tabs.size())) {
        tabSz = horiz ? _drag->src()->rect.width() : _drag->src()->rect.height();
        if (tabSz == 0) tabSz = 16 + 2 * MARGIN;
    }

    // Check if pointer is outside the strip by DETACH_DIST
    const bool outside = horiz ? (pos.y() < -DETACH_DIST || pos.y() > H + DETACH_DIST ||
                                  pos.x() < -DETACH_DIST || pos.x() > W + DETACH_DIST)
                                : (pos.x() < -DETACH_DIST || pos.x() > W + DETACH_DIST ||
                                   pos.y() < -DETACH_DIST || pos.y() > H + DETACH_DIST);

    if (outside && _rearrange == Rearrange::Externally) {
        // Escalate to QDrag for cross-window move
        TabItem* dragged = _drag->src();
        QPoint offset = _drag->pressOffset();
        _finishDrag(true); // cancel live drag first
        _pressedTab = nullptr;
        _startDrag(dragged, offset);
        return;
    }

    // Update ghost position — tab floats at cursor minus press offset
    const int coord = horiz ? pos.x() : pos.y();
    const int ghostCoord = coord - (horiz ? _drag->pressOffset().x() : _drag->pressOffset().y());
    const int ghostLimit = std::max(0, stripLen - tabSz);
    const int clampedGhost = std::clamp(ghostCoord, 0, ghostLimit);
    _drag->setGhostPos(horiz ? QPoint(clampedGhost, 0) : QPoint(0, clampedGhost));

    // Gap insertion point: midpoint of ghost drives where the drop slot opens
    if (!outside) {
        _drag->setDropPos(clampedGhost);
    } else {
        _drag->setDropPos({});
    }

    _doLayout();
    update();
}

// ── QSize hints ───────────────────────────────────────────────────────────────

QSize TabStrip::sizeHint() const {
    const bool horiz = (_orientation == Qt::Horizontal);
    QFontMetrics fm(font());
    const int iconSize = 16 + 2 * MARGIN;
    const int tabH = iconSize;// + 2 * MARGIN;

    int total = static_cast<int>(_tabs.size()) * (horiz ? iconSize * 2 : tabH);
    total += _plusButtonWidth();

    return horiz ? QSize(total, tabH) : QSize(tabH, total);
}

QSize TabStrip::minimumSizeHint() const {
    const bool horiz = (_orientation == Qt::Horizontal);
    const int iconSize = 16 + 2 * MARGIN;
    const int tabH = iconSize + MARGIN;// + 2 * MARGIN;
    const int minPerTab = iconSize / 2;

    int total = static_cast<int>(_tabs.size()) * minPerTab;
    total += _plusButtonWidth();

    return horiz ? QSize(total, tabH) : QSize(tabH, total);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void TabStrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QPalette& pal = palette();

    // Fill the full widget background so the drop-shadow effect wraps the
    // entire strip, not just the painted tabs.
    p.fillRect(rect(), pal.color(QPalette::Window));

    auto isDragSrc = [&](const TabItem& t) -> bool {
        // Hide the real tab when it's being dragged externally
        return _drag && _drag->src() == &t && _drag->srcStrip() != this;
    };

    bool firstVisible = true;
    for (auto& tabPtr : _tabs) {
        const TabItem& t = *tabPtr;
        if (isDragSrc(t) || t.rect.isEmpty()) continue;

        const QRect& r = t.rect;

        // Thin separator at left edge (skip the first visible tab)
        if (!firstVisible) {
            p.setPen(pal.color(QPalette::Mid));
            if (_orientation == Qt::Horizontal)
                p.drawLine(r.left(), r.top() + 4, r.left(), r.bottom() - 4);
            else
                p.drawLine(r.left() + 4, r.top(), r.right() - 4, r.top());
        }
        firstVisible = false;

        // Background
        if (t.active) {
            p.fillRect(r, pal.color(QPalette::Base));
        } else {
            p.fillRect(r, pal.color(QPalette::Window));
        }

        // Active indicator bar (bottom edge for horizontal)
        if (t.active) {
            if (_orientation == Qt::Horizontal) {
                p.fillRect(QRect(r.x(), r.bottom() - 1, r.width(), 2), pal.color(QPalette::Accent));
            } else {
                p.fillRect(QRect(r.right() - 1, r.y(), 2, r.height()), pal.color(QPalette::Accent));
            }
        }

        // Icon
        if (!t.icon.isNull() && !t.iconRect.isEmpty()) {
            t.icon.paint(&p, t.iconRect, Qt::AlignCenter, t.active ? QIcon::Normal : QIcon::Normal,
                         t.active ? QIcon::On : QIcon::Off);
        }

        // Label (with fade-out gradient if it overflows)
        if (t.labelVisible && !t.labelRect.isEmpty()) {
            p.save();
            p.setClipRect(t.labelRect);
            drawTabLabel(p, t, t.labelRect, pal.color(QPalette::WindowText), true);

            // Fade-out mask for overflow — stop above the active indicator bar (2px at bottom).
            // Extend 1px beyond labelRect.right() so the gradient reaches full opacity past
            // the clip edge, preventing sub-pixel text bleed at the boundary.
            const int fadeW = 16;
            if (p.fontMetrics().horizontalAdvance(t.label) > t.labelRect.width()) {
                const int fadeEnd = t.labelRect.right() + 1;
                const int fx = fadeEnd - fadeW;
                const int indicatorH = (t.active && _orientation == Qt::Horizontal) ? 2 : 0;
                const int fadeH = t.labelRect.height() - indicatorH;
                QLinearGradient g(fx, 0, fadeEnd, 0);
                QColor bg = pal.color(t.active ? QPalette::Base : QPalette::Window);
                g.setColorAt(0, QColor(bg.red(), bg.green(), bg.blue(), 0));
                g.setColorAt(1, bg);
                p.setClipRect(t.labelRect.adjusted(0, 0, 1, 0)); // widen clip to cover fadeEnd
                p.fillRect(QRect(fx, t.labelRect.y(), fadeW, fadeH), g);
            }
            p.restore();
        }

        // Close button — painted with the "close-button" resource icon;
        // Normal when hovered, Disabled when not to give a subtle hover effect.
        if (t.closeVisible && !t.closeRect.isEmpty()) {
            static const QIcon closeIcon(":/icons/close-button");
            const bool hovered = (&t == _hoveredCloseTab);
            closeIcon.paint(&p, t.closeRect, Qt::AlignCenter,
                            hovered ? QIcon::Normal : QIcon::Disabled,
                            QIcon::Off);
        }

        // Drag handle (⠿-style dots on right)
        if (t.handleVisible && !t.handleRect.isEmpty()) {
            p.save();
            p.setPen(pal.color(QPalette::Mid));
            const int dotR = 1;
            const int cx = t.handleRect.center().x();
            const int cy = t.handleRect.center().y();
            for (int row = -1; row <= 1; row++) {
                for (int col = -1; col <= 0; col++) {
                    p.drawEllipse(QPoint(cx + col * 4, cy + row * 4), dotR, dotR);
                }
            }
            p.restore();
        }
    }

    // Ghost tab — semi-transparent rendering of the dragged tab floating at the cursor
    if (_drag && _drag->ghostPos()) {
        const TabItem& src = *_drag->src();
        const bool horiz = (_orientation == Qt::Horizontal);
        const QPoint gp = *_drag->ghostPos();
        const QSize gsz = horiz ? QSize(src.rect.width(), height()) : QSize(width(), src.rect.height());
        const QRect ghostRect(gp, gsz);

        p.save();
        p.setOpacity(0.6);
        p.fillRect(ghostRect, pal.color(QPalette::Highlight).lighter(160));
        p.setPen(pal.color(QPalette::Highlight));
        p.drawRect(ghostRect.adjusted(0, 0, -1, -1));

        // Icon
        if (!src.icon.isNull()) {
            const QRect ir = src.iconRect.translated(ghostRect.topLeft() - src.rect.topLeft());
            src.icon.paint(&p, ir);
        }
        // Label
        drawTabLabel(p, src,
                     src.labelRect.translated(ghostRect.topLeft() - src.rect.topLeft()),
                     pal.color(QPalette::WindowText), true);
        p.restore();
    }
}

// ── Resize ────────────────────────────────────────────────────────────────────

void TabStrip::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    _doLayout();
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void TabStrip::mousePressEvent(QMouseEvent* event) {
    const QPoint pos = event->pos();
    TabItem* tab = _tabAtPoint(pos);

    if (event->button() == Qt::LeftButton) {
        if (tab) {
            // Check if user clicked the close button
            if (_showCloseBtn && tab->active && tab->closeRect.contains(pos)) {
                Q_EMIT tabCloseRequested(tab);
                return;
            }
            _pressedTab = tab;
            _pressPos = pos;
            _pressMoved = false;
            Q_EMIT tabSelectRequested(tab);
        }
    } else if (event->button() == Qt::MiddleButton) {
        if (tab) Q_EMIT tabCloseRequested(tab);
    } else if (event->button() == Qt::RightButton) {
        if (tab && _contextMenu) {
            Q_EMIT tabSelectRequested(tab);
            _contextMenu->popup(event->globalPosition().toPoint());
        }
    }
}

void TabStrip::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    _pressedTab = nullptr;
    if (_drag) _finishDrag();
}

void TabStrip::mouseMoveEvent(QMouseEvent* event) {
    // Update close-button hover state regardless of button press
    {
        const QPoint pos = event->pos();
        TabItem* hovered = nullptr;
        for (auto& t : _tabs) {
            if (t->closeVisible && t->closeRect.contains(pos)) {
                hovered = t.get();
                break;
            }
        }
        if (hovered != _hoveredCloseTab) {
            _hoveredCloseTab = hovered;
            update();
        }
    }

    if (!(event->buttons() & Qt::LeftButton) || !_pressedTab) return;

    const QPoint pos = event->pos();
    const QPoint delta = pos - _pressPos;

    if (!_pressMoved) {
        if (delta.manhattanLength() < DRAG_INIT_DIST) return;
        _pressMoved = true;
    }

    if (_rearrange == Rearrange::Never) return;

    // Both Internally and Externally use live mouse tracking.
    // Externally escalates to QDrag only when the pointer leaves the strip far enough.
    if (!_drag) {
        QPoint offset = _pressPos - _pressedTab->rect.topLeft();
        _drag = std::make_unique<TabDragState>(_pressedTab, offset, this);
        Q_EMIT dndBegin();
    }
    _updateDragDrop(pos);
}

void TabStrip::leaveEvent(QEvent*) {
    if (_hoveredCloseTab) {
        _hoveredCloseTab = nullptr;
        update();
    }
}

// ── Tooltip ───────────────────────────────────────────────────────────────────

bool TabStrip::event(QEvent* ev) {
    if (ev->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(ev);
        for (auto& tabPtr : _tabs) {
            const TabItem& t = *tabPtr;
            if (!t.rect.contains(he->pos()) || t.tooltip.isEmpty()) continue;
            // Only show when the label is actually clipped
            const bool clipped = !t.labelRect.isEmpty() &&
                                 fontMetrics().horizontalAdvance(t.label) > t.labelRect.width();
            if (clipped) {
                QToolTip::showText(he->globalPos(), t.tooltip, this, t.rect);
            } else {
                QToolTip::hideText();
            }
            return true;
        }
        QToolTip::hideText();
        return true;
    }
    return QWidget::event(ev);
}

// ── Drop events ───────────────────────────────────────────────────────────────

void TabStrip::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kTabMimeType)) {
        event->acceptProposedAction();

        // Record a drop position
        auto src = unpack_drop_source(event->mimeData());
        if (src) {
            auto* srcStrip = src->first;
            if (srcStrip->_drag) {
                srcStrip->_drag->setDstStrip(this);
            }
        }
        _updateDragDrop(event->position().toPoint());
    }
}

void TabStrip::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kTabMimeType)) {
        event->acceptProposedAction();
        _updateDragDrop(event->position().toPoint());
    }
}

void TabStrip::dragLeaveEvent(QDragLeaveEvent*) {
    update();
}

void TabStrip::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(kTabMimeType)) return;

    event->acceptProposedAction();

    auto src = unpack_drop_source(event->mimeData());
    if (!src) return;

    TabStrip* srcStrip = src->first;
    int srcIdx = src->second;
    QWidget* srcTab = srcStrip->getTabAt(srcIdx);
    if (!srcTab) return;

    // Determine drop index
    int dstIdx = static_cast<int>(_tabs.size()); // default: append
    if (srcStrip->_drag && srcStrip->_drag->dropIndex()) {
        dstIdx = *srcStrip->_drag->dropIndex();
    }

    if (srcStrip == this) {
        // Reorder within this strip
        if (_rearrange != Rearrange::Never && _reorderTab(srcIdx, dstIdx)) {
            int realTo = dstIdx - (dstIdx > srcIdx ? 1 : 0);
            Q_EMIT tabRearranged(srcIdx, realTo);
        }
        if (srcStrip->_drag) srcStrip->_drag->setDropIndex({}); // handled
    } else if (_rearrange == Rearrange::Externally || srcStrip->_rearrange == Rearrange::Externally) {
        // Cross-strip move
        Q_EMIT tabMoveRequested(srcTab, srcIdx, srcStrip, dstIdx);
        if (srcStrip->_drag) srcStrip->_drag->setDropIndex({}); // handled
    }

    update();
}

} // namespace Linea::UI
