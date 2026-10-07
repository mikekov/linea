// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * CanvasFrame — container widget hosting rulers around a canvas stack.
 *
 * Layout:
 *   [ empty corner | HRuler ]
 *   [ VRuler       | canvas ]
 *
 * The canvas area is a QStackedWidget so multiple canvases (one per document)
 * can share the same rulers. Only the visible canvas receives mouse events
 * and drives the ruler cursor position.
 */

#ifndef LINEA_UI_CANVAS_FRAME_H
#define LINEA_UI_CANVAS_FRAME_H

#include <QWidget>
#include <sigc++/scoped_connection.h>

#include "ruler-widget.h"

class QStackedWidget;
class SPDocument;

namespace Inkscape {
class Selection;
}

namespace Inkscape::UI::Widget {
class Canvas;
}
class SPDesktop;

namespace Linea::UI {

class CanvasFrame : public QWidget {
    Q_OBJECT

public:
    explicit CanvasFrame(QWidget* parent = nullptr);
    ~CanvasFrame() override;

    /// Add a canvas to the stack. It will drive the rulers when visible.
    void addCanvas(Inkscape::UI::Widget::Canvas* canvas);
    /// Remove a canvas from the stack.
    void removeCanvas(Inkscape::UI::Widget::Canvas* canvas);
    /// Show the given canvas (switch current page), or the "no document"
    /// empty page when @p canvas is null.
    void setCurrentCanvas(SPDesktop* desktop, Inkscape::UI::Widget::Canvas* canvas);
    /// Get the currently visible canvas.
    Inkscape::UI::Widget::Canvas* currentCanvas() const;

    /// Update ruler data (unit, range, page, selection) from the current desktop.
    void updateRulers();

    /// Show or hide the rulers (and corner). The preference is stored, but
    /// the rulers are only actually shown while a desktop is active.
    void setRulersVisible(bool visible);
    bool rulersVisible() const { return _rulersVisible; }

    /// Whether the rulers are on screen right now (preference + a desktop).
    bool rulersShown() const { return _rulersVisible && _desktop != nullptr; }

    /// Ruler thickness in pixels. Collapsible panels can use this to adjust margins.
    int rulerSize() const { return _hruler->rulerSize(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // set current desktop (may be null)
    void setDesktop(SPDesktop* desktop);
    void applyRulerVisibility();
    void onCanvasMouseMoved(QPointF pos);
    void onCanvasMouseLeft();
    void onRulerDragStarted(RulerWidget* ruler, double position);
    void setRulersUnit(const QString& abbr);

    QStackedWidget* _stack = nullptr;
    QWidget* _emptyPage = nullptr; // "no document" placeholder page, first in the stack
    RulerWidget* _hruler = nullptr;
    RulerWidget* _vruler = nullptr;
    QWidget* _corner = nullptr;
    bool _rulersVisible = true;
    QPointF _lastMousePos;
    SPDesktop* _desktop = nullptr;
    bool _rulerDragActive = false;            // true while a ruler-initiated guide drag holds the mouse grab
    sigc::scoped_connection _pageSelectedConn;
    sigc::scoped_connection _pageModifiedConn;
    sigc::scoped_connection _selModifiedConn;
    sigc::scoped_connection _selChangedConn;
};

} // namespace Linea::UI

#endif // LINEA_UI_CANVAS_FRAME_H
