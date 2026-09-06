// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_WIDGET_OPTGLAREA_H
#define INKSCAPE_UI_WIDGET_OPTGLAREA_H

#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <epoxy/gl.h>

#include <QOpenGLWidget>
#include <QOpenGLWindow>
#include <QOpenGLFunctions>

namespace Cairo { class Context; }
// namespace Gdk { class GLContext; }

namespace Inkscape::UI::Widget {

/**
 * A widget that can dynamically switch between a Gtk::DrawingArea and a Gtk::GLArea.
 * Based on the source code for both widgets.
 */
class OptGLArea : public QOpenGLWidget, protected QOpenGLFunctions // Gtk::Widget
{
public:
    OptGLArea();
    ~OptGLArea() override;

    /**
     * Set whether OpenGL is enabled. Initially it is disabled. Upon enabling it,
     * create_context will be called as soon as the widget is realized. If
     * context creation fails, OpenGL will be disabled again.
     */
    void set_opengl_enabled(bool);
    bool get_opengl_enabled() const { return opengl_enabled; }

    /**
     * Call before doing any OpenGL operations to make the context current.
     * Automatically done before calling paint_widget().
     */
    void make_current();

    /**
     * Call before rendering to the widget to bind the widget's framebuffer.
     */
    void bind_framebuffer();

    // void snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot) override;

protected:
    virtual void on_realize();
    virtual void on_unrealize();

    // OpenGL lifecycle
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    /**
     * Reimplement to create the desired OpenGL context. Return nullptr on error.
     */
    // virtual Glib::RefPtr<Gdk::GLContext> create_context() = 0;
    virtual QOpenGLContext* create_context() = 0;

    /**
     * Reimplement to render the widget. When OpenGL is enabled, the Cairo context is used to
     * render to an off-screen surface which is then blitted to the screen.
     * Returns true if rendering was performed, false if the widget is not ready to paint.
     */
    virtual bool paint_widget(const QOpenGLContext&, Cairo::RefPtr<Cairo::Context>& ctx) { return false; }

    /* QT-specific start */
    // GTK compatibility stubs
    bool get_realized() const { return _ready; }
    int get_width() const { return width(); }
    int get_height() const { return height(); }
    int get_scale_factor() const { return devicePixelRatio(); }
    // Qt equivalent of GTK's queue_draw()
    void queue_draw() { update(); }
    void queue_draw_area(int x, int y, int w, int h) { update(x, y, w, h); }
    /* QT-specific end */
private:
    bool opengl_enabled = true;
    bool cairo_renderer = true; // Cairo renders entire scene
    bool _ready = false;

    struct GLState;
    Cairo::RefPtr<Cairo::ImageSurface> _surface;
    std::shared_ptr<GLState> gl;

    void init_opengl();
    void uninit_opengl();
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_OPTGLAREA_H
