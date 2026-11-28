// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#ifndef LINEA_UI_WIDGET_UTILS_H
#define LINEA_UI_WIDGET_UTILS_H

#include <QObject>
#include <QPoint>
#include <QSize>
#include <functional>
#include <vector>

class QAbstractButton;
class QLineEdit;
class QColor;
class QPainter;
class QRect;
class QScrollArea;
class QScrollBar;
class QString;
class QWidget;
class QLayout;

namespace Linea::UI {

constexpr QSize defaultButtonSize = {24, 24};

/**
 * Apply left/right padding as content margins to a widget's layout, leaving
 * the top/bottom margins untouched. Used to inset child panels of properties
 * panels. Does nothing if the widget has no layout.
 */
void applyHorizontalPadding(QWidget* widget, int left, int right);

/**
 * Keep `follower`'s visibility in sync with `leader`'s. An event filter is
 * installed on `leader` so that any subsequent setVisible() call on it (e.g.
 * by the Binder's visibility rules) is mirrored onto `follower`. The follower
 * is also synced immediately to the leader's current state.
 *
 * The filter object is parented to `follower`, so it is destroyed with it.
 */
void syncVisibility(QWidget* follower, QWidget* leader);

/**
 * Same as syncVisibility, but `follower` stays visible while ANY of the
 * `leaders` is visible — it hides only when all of them are hidden.
 */
void syncVisibilityAny(QWidget* follower, std::vector<QWidget*> leaders);

/**
 * Fix the size of every direct `QToolButton` child sitting in `column` of the
 * widget's `mainGrid` to `size` (default 24x24). Stylesheet size constraints
 * can't do this under the macOS style — it treats them as contents-box sizes
 * — so the panels apply it in code after setupUi(). Call from each widget's
 * ctor; finds `mainGrid` anywhere in the child tree.
 */
void fixColumnToolButtons(QWidget* widget, int column = 5, QSize size = defaultButtonSize);

/**
 * Give `edit`'s embedded action icons (added via QLineEdit::addAction) a
 * hover state Qt doesn't provide: the Midlight background color fades in
 * behind the icon and a pointing-hand cursor is shown. Does nothing while
 * the line edit is disabled. Call after addAction(); an event filter is
 * installed on each embedded button, parented to it.
 */
void highlightEmbeddedIconsOnHover(QLineEdit* edit);

/**
 * Companion for a QScrollArea that hides the vertical scroll bar whenever
 * there is nothing to scroll (maximum() == 0):
 *
 * - Reserved (default): for a QScrollArea using Qt::ScrollBarAlwaysOn — keeps
 *   the gutter reserved so content width is stable, but hides the vertical
 *   scroll bar itself whenever there is nothing to scroll. The scroll area
 *   force-shows the bar on every layout pass, so visibility is re-applied
 *   after LayoutRequest/Resize events on the area and its viewport. Only
 *   manages the bar while the policy is ScrollBarAlwaysOn — switching the
 *   policy (e.g. ScrollBarAlwaysOff) is left to Qt.
 *
 * - Overlay: sets Qt::ScrollBarAlwaysOff on the scroll area so no gutter space
 *   is reserved, and floats a vertical QScrollBar directly over the right edge
 *   of the viewport whenever content is scrollable.
 *
 * Parented to the scroll area; construct it and forget it. setForceHidden()
 * additionally suppresses the bar even when content is scrollable.
 */
class ScrollBarAutoHide : public QObject {
public:
    enum Mode { Reserved, Overlay };

    explicit ScrollBarAutoHide(QScrollArea* scrollArea, Mode mode = Reserved);
    void setForceHidden(bool hidden);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void apply();
    void scheduleApply();
    void updateOverlayGeometry();

    QScrollArea* _scrollArea;
    QScrollBar* _overlayBar = nullptr;
    Mode _mode;
    bool _forceHidden = false;
    bool _applyScheduled = false;
};

/**
 * Restore `window`'s position and size from preferences under `prefsPath`
 * (keys x, y, width, height) and persist them on subsequent move, resize,
 * and close. If the saved position lands on a display that is not
 * connected — or leaves the title bar off-screen — the window is moved
 * back onto a visible screen (largest overlap, else the parent's screen).
 *
 * `defaultSize` is used when no usable size was stored yet; pass an
 * invalid QSize to keep the widget's own default.
 *
 * An event filter is installed on `window`, parented to it, so the
 * tracking dies with the window.
 */
void persistGeometry(QWidget* window, const char* prefsPath, QSize defaultSize = {});

/**
 * Clamp `pos` so a popup of `size` placed there stays inside the available
 * geometry of the screen containing that point (primary screen if none),
 * keeping a small margin from the screen edges.
 */
void ensurePopupOnScreen(QPoint& pos, QSize size);

/**
 * Draw `text` in `color`, clipped to and aligned inside `rect`. If the text
 * is wider than the rect, the right edge fades out via an alpha gradient
 * instead of eliding with "..." (the painter's font must already be set).
 */
void paintFadedText(QPainter& painter, const QRect& rect, const QString& text,
                    const QColor& color, Qt::Alignment alignment);

/**
 * Settle `widget`'s layout geometry synchronously after visibility changes.
 *
 * setVisible() takes effect at the next paint, but the geometry it implies
 * only settles when queued LayoutRequests are delivered on a later
 * event-loop pass — a repaint landing in between shows widgets at their old
 * positions with gaps where hidden rows were.
 *
 * Pending Polish/FontChange/StyleChange events are delivered to the parent
 * subtree first — QLabel caches its sizeHint until FontChange, so a layout
 * run before delivery computes stale geometry and re-runs (visibly) when
 * the events finally arrive. Then LayoutRequests are delivered to the
 * widget and each ancestor, innermost first — the same work the event loop
 * would do a frame later. (layout->activate() alone is NOT equivalent: it
 * does not re-assign widget sizes from updated sizeHints — the
 * LayoutRequest path does; and a global sendPostedEvents(nullptr, ...)
 * drains only the queue as-posted, while resizes during the flush post new
 * requests.)
 */
void settleLayout(QWidget* widget);

class Expander;

/**
 * Wire `expander` to expand/collapse `containers` with a smooth height
 * animation, seeding `expander`'s state from preferences under `prefsPath`
 * (if non-empty) and persisting it on toggle. All containers share one
 * state and animate in sync — they make up a single logical section.
 * The expander owns its own icon/state — clicking it flips the section.
 *
 * Returns a predicate querying the current expanded state (suitable for
 * composing into `Binder::visibleWhen` rules alongside other conditions).
 */
std::function<bool()> setupCollapsibleSection(Expander* expander, std::vector<QWidget*> containers,
                                              const char* prefsPath = nullptr, bool defaultExpanded = false);

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_UTILS_H
