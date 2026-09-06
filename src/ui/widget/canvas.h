// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Inkscape canvas widget.
 */
/*
 * Authors:
 *   Tavmjong Bah
 *   PBS <pbs3141@gmail.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_CANVAS_H
#define INKSCAPE_UI_WIDGET_CANVAS_H

#include <2geom/rect.h>

/* GTK-specific start
#include <gtkmm/gesture.h> // Gtk::EventSequenceState
GTK-specific end */

/* QT-specific start */
#include <epoxy/gl.h>
#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QNativeGestureEvent>
#include <functional>
#include "display/control/canvas-item-context.h"

#include <QRect>
#include <QVariant>
QT_BEGIN_NAMESPACE
class QMouseEvent;
class QTabletEvent;
class QWheelEvent;
class QKeyEvent;
class QEvent;
class QInputMethodEvent;
QT_END_NAMESPACE
/* QT-specific end */

#include "display/rendermode.h"
#include "events/enums.h"

/* GTK-specific start*/
#include "optglarea.h"
#include <sigc++/scoped_connection.h>
/* GTK-specific end */

/* QT-specific start */
#include <sigc++/signal.h>
/* QT-specific end */

/* GTK-specific start
namespace Gdk {
class Rectangle;
} // namespace Gdk

namespace Gtk {
class EventControllerKey;
class EventControllerMotion;
class EventControllerScroll;
class GestureClick;
} // namespace Gtk
GTK-specific end */

class SPDesktop;

namespace Inkscape {

class CanvasItem;
class CanvasItemCatchall;
class CanvasItemGroup;
class CanvasItemDrawing;
class Drawing;
class ButtonPressEvent;
class ButtonReleaseEvent;
class MotionEvent;

namespace Colors::CMS {
    class TransformCairo;
}

namespace UI::Widget {

class CanvasPrivate;

/**
 * A widget for Inkscape's canvas.
 */

class Canvas final : public OptGLArea
{
    using parent_type = OptGLArea;

public:
    Canvas();
    ~Canvas() final;

    /* Configuration */

    // Desktop (Todo: Remove.)
    void set_desktop(SPDesktop *desktop) { _desktop = desktop; }
    SPDesktop *get_desktop() const { return _desktop; }

    // Drawing
    void set_drawing(Inkscape::Drawing *drawing);

    // Canvas item root
    CanvasItemGroup *get_canvas_item_root() const;

    // Geometry
    void set_pos   (const Geom::IntPoint &pos);
    void set_pos   (const Geom::Point    &fpos) { set_pos(fpos.round()); }
    void set_affine(const Geom::Affine   &affine);

/* GTK-specific start
    const Geom::IntPoint &get_pos   () const { return _pos; }
GTK-specific end */

/* QT-specific start */
    Geom::Point get_pos   () const { return _pos; }
/* QT-specific end */

    const Geom::Affine   &get_affine() const { return _affine; }
    const Geom::Affine   &get_geom_affine() const; // tool-base.cpp (todo: remove this dependency)

    // Background
    void set_desk  (uint32_t rgba);
    void set_shadow(uint32_t rgba, float size);
    void set_page  (uint32_t rgba);

/* QT-specific start */
    // Additional Qt-specific methods for desktop.cpp compatibility
    void setDesktop(SPDesktop* desktop);
    // SPDesktop* desktopAdapter() const { return _desktop; }

    // Canvas item management
    class CanvasItemContext* canvasItemContext() const;
    void setupCanvasItems();

    // Canvas item groups
    CanvasItemGroup* canvasGroupControls() const;
    CanvasItemGroup* getCanvasControls() const { return canvasGroupControls(); }
    CanvasItemGroup* canvasGroupDrawing() const;
    CanvasItemGroup* canvasGroupGrids() const;
    CanvasItemGroup* getCanvasGrids() const { return canvasGroupGrids(); }
    CanvasItemGroup* canvasGroupGuides() const;
    CanvasItemGroup* getCanvasGuides() const { return canvasGroupGuides(); }
    CanvasItemGroup* canvasGroupTemp() const;
    CanvasItemGroup* getCanvasTemp() const { return canvasGroupTemp(); }
    CanvasItemGroup* canvasGroupSketch() const;
    CanvasItemGroup* getCanvasSketch() const { return canvasGroupSketch(); }
    CanvasItemGroup* getCanvasPagesBg() const { return _canvas_group_pages_bg; }
    CanvasItemGroup* getCanvasPagesFg() const { return _canvas_group_pages_fg; }
    CanvasItemCatchall* getCanvasCatchall() const { return _canvas_catchall; }

    // Page info
    // void setPageInfo(struct PageInfo pi);
    void setColours(uint32_t page, uint32_t desk, uint32_t border);

    // Zoom control
    // void set_zoom(double factor);

    // Scroll
    void scrollBy(int dx, int dy);
    void scrollTo(const Geom::Point& pos);

    // Coordinate transforms (additional overloads)
    Geom::Point windowToCanvas(const QPointF& windowPos) const;
    QPoint canvasToWindow(const Geom::Point& canvasPos) const;
    Geom::Affine windowToCanvasTransform() const;
    Geom::Affine canvasToWindowTransform() const;

    // Event handling
    void handleKeyPress(QKeyEvent* event);
    void handleKeyRelease(QKeyEvent* event);
/* QT-specific end */

    //  Rendering modes
    void set_render_mode(Inkscape::RenderMode mode);
    void set_color_mode (Inkscape::ColorMode  mode);
    void set_split_mode (Inkscape::SplitMode  mode);
    Inkscape::RenderMode get_render_mode() const { return _render_mode; }
    Inkscape::ColorMode  get_color_mode()  const { return _color_mode; }
    Inkscape::SplitMode  get_split_mode()  const { return _split_mode; }
    void set_clip_to_page_mode(bool clip);
    void set_antialiasing_enabled(bool enabled);

    // CMS
    void set_cms_active(bool active) { _cms_active = active; }
    bool get_cms_active() const { return _cms_active; }

    /* QT-specific start */
    // GTK compatibility stubs
    // bool get_realized() const { return _ready; }
    // int get_width() const { return width(); }
    // int get_height() const { return height(); }
    // int get_scale_factor() const { return devicePixelRatio(); }
    /* QT-specific end */

    /* Observers */

    // Geometry
    Geom::IntPoint get_dimensions() const;
    bool world_point_inside_canvas(Geom::Point const &world) const; // desktop-events.cpp
    Geom::Point canvas_to_world(Geom::Point const &window) const;
    Geom::IntRect get_area_world() const;
    bool canvas_point_in_outline_zone(Geom::Point const &world) const;

    // State
    bool is_dragging() const { return _is_dragging; } // selection-chemistry.cpp

    void blink();
    // Mouse
    std::optional<Geom::Point> get_last_mouse() const; // desktop-widget.cpp

    /* Methods */

    // Invalidation
    void redraw_all();                  // Mark everything as having changed.
    void redraw_area(Geom::Rect const &area); // Mark a rectangle of world space as having changed.
    void redraw_area(int x0, int y0, int x1, int y1);
    void redraw_area(Geom::Coord x0, Geom::Coord y0, Geom::Coord x1, Geom::Coord y1);
    void request_update();              // Mark geometry as needing recalculation.

    // Callback run on destructor of any canvas item
    void canvas_item_destructed(Inkscape::CanvasItem *item);

    // State
    Inkscape::CanvasItem *get_current_canvas_item() const { return _current_canvas_item; }
    void                  set_current_canvas_item(Inkscape::CanvasItem *item) {
        _current_canvas_item = item;
    }
    Inkscape::CanvasItem *get_grabbed_canvas_item() const { return _grabbed_canvas_item; }
    void                  set_grabbed_canvas_item(Inkscape::CanvasItem *item, EventMask mask) {
        _grabbed_canvas_item = item;
        _grabbed_event_mask = mask;
    }
    void set_all_enter_events(bool on) { _all_enter_events = on; }

    void enable_autoscroll();

    // Additional methods required by desktop.cpp
    bool get_opengl_enabled() const { return false; } // Qt canvas always uses OpenGL for output

/* GTK-specific start
    void set_cursor(Glib::RefPtr<Gdk::Cursor> const &cursor);
    void grab_focus();
    bool has_focus() const;
GTK-specific end */

/* QT-specific start */
    void set_cursor(const std::string& cursor_name);
    void set_cursor(Qt::CursorShape shape);
    void grab_focus();
    bool has_focus() const;

    // Qt equivalent of GTK's queue_draw()
/* QT-specific end */

    // Mouse position callbacks for ruler integration
    std::function<void(QPointF)> on_mouse_moved;
    std::function<void()> on_mouse_left;

    sigc::connection connectFocusIn(sigc::slot<void ()> &&slot) { return _signal_focus_in.connect(std::move(slot)); }
    sigc::connection connectFocusOut(sigc::slot<void ()> &&slot) { return _signal_focus_out.connect(std::move(slot)); }
    sigc::connection connectIMCommit(sigc::slot<void (QString const &)> &&slot) { return _signal_im_commit.connect(std::move(slot)); }

    // Input method support
    void setIMCursorRect(QRect rect);
    void resetIM();

private:

    // Common event handlers (no platform-specific types)
    void on_focus_in();
    void on_focus_out();

/* GTK-specific start
    // EventControllerScroll
    bool on_scroll(Gtk::EventControllerScroll const &controller, double dx, double dy);

    // GestureClick
    Gtk::EventSequenceState on_button_pressed (Gtk::GestureClick const &controller,
                                               int n_press, double x, double y);
    Gtk::EventSequenceState on_button_released(Gtk::GestureClick const &controller, int n_press, double x, double y,
                                               int button);

    // EventControllerMotion
    void on_motion(Gtk::EventControllerMotion const &controller, double x, double y);
    void on_enter (Gtk::EventControllerMotion const &controller, double x, double y);
    void on_leave (Gtk::EventControllerMotion const &controller);

    sigc::scoped_connection blinking;

    // EventControllerKey
    bool on_key_pressed(Gtk::EventControllerKey &controller, unsigned keyval, unsigned keycode,
                        Gdk::ModifierType state);
    void on_key_released(Gtk::EventControllerKey &controller, unsigned keyval, unsigned keycode,
                         Gdk::ModifierType state);

    void on_realize() final;
    void on_unrealize() final;
    void size_allocate_vfunc(int width, int height, int baseline) final;

    Glib::RefPtr<Gdk::GLContext> create_context() final;
GTK-specific end */
    bool paint_widget(const QOpenGLContext& context, Cairo::RefPtr<Cairo::Context>& ctx) final;

/* QT-specific start */
    void on_realize() final;
    void on_unrealize() final;
    bool focusNextPrevChild(bool next) override;

    // Qt event handlers
    void mousePressEvent(QMouseEvent *event) override;
    void tabletEvent(QTabletEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    // OpenGL context
    QOpenGLContext* create_context() final;
    void ensureWidgetFbo(int w, int h);

    // Size allocation
    void size_allocate(int width, int height);

    // Event handlers (called from Qt event methods)
    void on_enter(double x, double y, int state);
    void on_leave(int state);
    void on_button_pressed(ButtonPressEvent& event, int n_press);
    void on_button_released(ButtonReleaseEvent& event);
    void on_motion(MotionEvent& event);
    bool on_scroll(double dx, double dy);
    bool on_key_pressed(int keyval, int keycode, int state);
/* QT-specific end */

    /* Configuration */

    // Desktop
    SPDesktop *_desktop = nullptr;

    // Drawing
    Inkscape::Drawing *_drawing = nullptr;

    // Geometry

    Geom::IntPoint _pos = {0, 0}; ///< Coordinates of top-left pixel of canvas view within canvas.
    Geom::Affine _affine; ///< The affine that we have been requested to draw at.

    // Rendering modes
    Inkscape::RenderMode _render_mode = Inkscape::RenderMode::NORMAL;
    Inkscape::SplitMode  _split_mode  = Inkscape::SplitMode::NORMAL;
    Inkscape::ColorMode  _color_mode  = Inkscape::ColorMode::NORMAL;
    bool _antialiasing_enabled = true;

    // CMS
    bool _cms_active = false;
    std::shared_ptr<Colors::CMS::TransformCairo> _cms_transform; ///< The lcms transform to apply to canvas.

    void set_cms_transform(); ///< Set the lcms transform.

/* QT-specific start */
    // Qt-specific members for desktop.cpp compatibility
    double _zoom = 1.0;
    double _rotation = 0.0;

    // Canvas item context and groups
    CanvasItemGroup* _canvas_group_controls = nullptr;
    CanvasItemGroup* _canvas_group_drawing = nullptr;
    CanvasItemGroup* _canvas_group_grids = nullptr;
    CanvasItemGroup* _canvas_group_guides = nullptr;
    CanvasItemGroup* _canvas_group_temp = nullptr;
    CanvasItemGroup* _canvas_group_sketch = nullptr;
    CanvasItemGroup* _canvas_group_pages_bg = nullptr;
    CanvasItemGroup* _canvas_group_pages_fg = nullptr;
    CanvasItemCatchall* _canvas_catchall = nullptr;
/* QT-specific end */

    /* Internal state */

    // Event handling/item picking
    bool     _left_grabbed_item; ///< Relied upon by connector tool.
    bool     _all_enter_events;  ///< Keep all enter events. Only set true in connector-tool.cpp.
    bool     _is_dragging;       ///< Used in selection-chemistry to block undo/redo.
    int      _state;             ///< Last known modifier state (SHIFT, CTRL, etc.).

    Inkscape::CanvasItem *_current_canvas_item;     ///< Item containing cursor, nullptr if none.
    Inkscape::CanvasItem *_current_canvas_item_new; ///< Item to become _current_item, nullptr if none.
    Inkscape::CanvasItem *_grabbed_canvas_item;     ///< Item that holds a pointer grab; nullptr if none.
    EventMask _grabbed_event_mask;

    // Drawing
    bool _need_update = true; // Set true so setting CanvasItem bounds are calculated at least once.

    // Split view
    Inkscape::SplitDirection _split_direction;
    Geom::Point _split_frac;
    Inkscape::SplitDirection _hover_direction;
    bool _split_dragging;
    Geom::IntPoint _split_drag_start;

    sigc::signal<void ()> _signal_resize;
    sigc::signal<void ()> _signal_focus_in;
    sigc::signal<void ()> _signal_focus_out;
    sigc::signal<void (QString const &)> _signal_im_commit;

    QRect _im_cursor_rect;

    void update_cursor();

    // Opaque pointer to implementation
    friend class CanvasPrivate;
    std::unique_ptr<CanvasPrivate> d;
};

} // namespace UI::Widget

} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_CANVAS_H
