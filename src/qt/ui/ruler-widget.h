// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * RulerWidget — Qt ruler widget indicating cursor position along an axis.
 *
 * Port of ink-ruler.cpp (which came originally from Gimp).
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_RULER_WIDGET_H
#define LINEA_UI_RULER_WIDGET_H

#include <QWidget>

#include "util/units.h"

class QActionGroup;
class QMenu;

namespace Linea::UI {

/**
 * Ruler widget that shows ticks, labels, page/selection ranges,
 * and a position marker following the cursor.
 *
 * The vertical ruler can draw in either increasing or decreasing
 * Y direction: pass a range where upper < lower to flip the axis.
 */
class RulerWidget : public QWidget {
    Q_OBJECT

public:
    explicit RulerWidget(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~RulerWidget() override;

    void setUnit(const Inkscape::Util::Unit* unit);
    const Inkscape::Util::Unit* unit() const { return _unit; }

    void setRange(double lower, double upper);
    void setPage(double lower, double upper);
    void setSelection(double lower, double upper);

    /// Set the cursor position in widget-local pixel coordinates.
    void setCursorPosition(double position);

    /// Show or hide the position marker.
    void setMarkerVisible(bool visible);

    /// Ruler thickness in pixels
    void setRulerSize(int size);
    int rulerSize() const { return _ruler_size; }

    /// Orientation of this ruler (horizontal or vertical).
    Qt::Orientation orientation() const { return _orientation; }

    QSize sizeHint() const override;

Q_SIGNALS:
    /// Emitted when the user picks a unit from the context menu.
    void unitChanged(const QString& abbr);

    /// Emitted once the left button has been pressed and dragged past the
    /// drag-tolerance threshold. The position is in widget-local pixels along
    /// the ruler's axis. Connect to this to begin a guide-drag operation.
    void rulerDragStarted(RulerWidget* ruler, double position);

    /// Emitted on a left-button click that released without exceeding the
    /// drag-tolerance threshold (i.e. not a drag). Connect to toggle guide
    /// visibility, matching the GTK ruler behavior.
    void rulerClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void drawRuler(QPainter& painter);
    void drawMarker(QPainter& painter);
    void createContextMenu();

    QMenu* _menu = nullptr;
    QActionGroup* _unit_action_group = nullptr;
    const Qt::Orientation _orientation;
    const Inkscape::Util::Unit* _unit = nullptr;
    double _lower = 0;
    double _upper = 1000;
    double _position = 0;
    double _max_size = 1000;

    // Page block
    double _page_lower = 0;
    double _page_upper = 0;

    // Selection block
    double _sel_lower = 0;
    double _sel_upper = 0;

    bool _marker_visible = false;
    int _ruler_size = 20;

    // Left-button press tracking for drag-vs-click discrimination.
    bool _pressed = false;       ///< True between left press and release.
    bool _dragged = false;       ///< True once movement exceeded drag tolerance.
    QPointF _press_pos;          ///< Widget-local position of the press.
};

} // namespace Linea::UI

#endif // LINEA_UI_RULER_WIDGET_H
