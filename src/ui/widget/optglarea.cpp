// SPDX-License-Identifier: GPL-2.0-or-later

#include "optglarea.h"
#include <2geom/int-point.h>

#include "ui/widget/canvas/texture.h"
#include <cairomm/cairomm.h>
#include <cairomm/context.h>
#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <cstdio>
#include <sys/stat.h>

namespace Inkscape::UI::Widget {
namespace {

template <auto &f>
GLuint create_buffer()
{
    GLuint result;
    f(1, &result);
    return result;
}

template <typename T>
std::weak_ptr<T> weakify(std::shared_ptr<T> const &p)
{
    return p;
}

// Workaround for sigc not supporting move-only lambdas.
template <typename F>
auto share_lambda(F &&f)
{
    using Fd = std::decay_t<F>;

    struct Result
    {
        auto operator()() { (*f)(); }
        std::shared_ptr<Fd> f;
    };

    return Result{std::make_shared<Fd>(std::move(f))};
}

} // namespace

struct OptGLArea::GLState
{
    QOpenGLContext *context = nullptr;
    OptGLArea *parent = nullptr;

    GLuint framebuffer = 0;
    GLuint stencilbuffer = 0;

    std::optional<Geom::IntPoint> size;

    Texture current_texture;
    std::vector<Texture> spare_textures;

    GLState(OptGLArea* parent_, QOpenGLContext* context_)
        : parent(parent_), context(context_)
    {
        parent->glGenFramebuffers(1, &framebuffer);
        parent->glGenRenderbuffers(1, &stencilbuffer);
        // builder->set_context(context);
        // builder->set_format(Gdk::MemoryFormat::B8G8R8A8_PREMULTIPLIED);
    }

    ~GLState()
    {
        parent->glDeleteRenderbuffers(1, &stencilbuffer);
        parent->glDeleteFramebuffers(1, &framebuffer);
    }
};

OptGLArea::OptGLArea() = default;
OptGLArea::~OptGLArea() = default;

void OptGLArea::on_realize()
{
    // Gtk::Widget::on_realize();
    if (opengl_enabled) init_opengl();
}

void OptGLArea::on_unrealize()
{
    if (opengl_enabled) uninit_opengl();
    // Gtk::Widget::on_unrealize();
}

void OptGLArea::set_opengl_enabled(bool enabled)
{
    if (opengl_enabled == enabled) return;
    if (opengl_enabled && get_realized()) uninit_opengl();
    opengl_enabled = enabled;
    if (opengl_enabled && get_realized()) init_opengl();
}

void OptGLArea::init_opengl()
{
    auto context = create_context();
    if (!context) {
        opengl_enabled = false;
        return;
    }

    // context->makeCurrent();
    makeCurrent();
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    gl = std::make_shared<GLState>(this, context);
    // Gdk::GLContext::clear_current();
}

void OptGLArea::uninit_opengl()
{
    // gl->context->make_current();
    makeCurrent();
    gl.reset();
    // Gdk::GLContext::clear_current();
}

void OptGLArea::make_current()
{
    assert(gl);
    makeCurrent();
    // gl->context->make_current();
}

void OptGLArea::bind_framebuffer()
{
    assert(gl);
    assert(gl->current_texture);

    glBindFramebuffer(GL_FRAMEBUFFER, gl->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->current_texture.id(), 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl->stencilbuffer);
}

void OptGLArea::initializeGL() {
    initializeOpenGLFunctions();

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Mark as ready BEFORE calling on_realize so derived classes can check get_realized()
    _ready = true;

    on_realize();
    // d->graphics->set_scale_factor(static_cast<int>(devicePixelRatio()));
    // d->graphics->set_outlines_enabled(false);
    // d->graphics->set_background_in_stores(false);
    // d->graphics->set_colours(d->_pageColor, d->_deskColor, d->_borderColor);

    // setupCanvasItems();

    // Trigger a repaint after initialization is complete
    queue_draw();
}

void OptGLArea::resizeGL(int w, int h) {
    if (!gl) return;
    // Reset cached size so paintGL will recreate framebuffer with new dimensions
    // Use -1,-1 to force recreation on next paint
    gl->size = Geom::IntPoint(-1, -1);

    // Trigger repaint to update the framebuffer
    queue_draw();
}

void OptGLArea::paintGL()
{
    if (!_ready || !gl) return;

// std::cout << "DEBUG: paintGL() called" << std::endl;
    auto const size = Geom::IntPoint(width(), height()) * get_scale_factor();
    if (size.x() == 0 || size.y() == 0) return;

    if (cairo_renderer) {
        if (!_surface || _surface->get_width() != size.x() || _surface->get_height() != size.y()) {
            _surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, size.x(), size.y());
            _surface->set_device_scale(get_scale_factor(), get_scale_factor());
        }
        auto ctx = Cairo::Context::create(_surface);
        auto painted = paint_widget(*context(), ctx);

        if (!painted) {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }

        // 3. Upload to GL texture and blit it to the screen.
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, _surface->get_stride() / 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x(), size.y(), 0,
                     GL_BGRA, GL_UNSIGNED_BYTE, _surface->get_data());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        // Cairo's surface data is top-down, while OpenGL's framebuffer origin is bottom-up,
        // so we flip the Y axis during the blit.
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
        glBlitFramebuffer(0, 0, size.x(), size.y(),
                          0, size.y(), size.x(), 0,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        glDeleteFramebuffers(1, &fbo);

        glDeleteTextures(1, &tex);
        return;
    }

    // QOpenGLWidget has already made its context current before calling paintGL().

    // Check if the size has changed.
    if (size != gl->size) {
        gl->size = size;

        // Resize the stencil buffer to match.
        glBindRenderbuffer(GL_RENDERBUFFER, gl->stencilbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x(), size.y());
    }

    // Discard wrongly-sized spare textures.
    std::erase_if(gl->spare_textures, [&] (auto &tex) { return tex.size() != size; });
    // Todo: Consider clearing out excess spare textures every once in a while.

    // Set the current texture (the one paint_widget() will draw into via bind_framebuffer()).
    assert(!gl->current_texture);
    if (!gl->spare_textures.empty()) {
        gl->current_texture = std::move(gl->spare_textures.back());
        gl->spare_textures.pop_back();
    } else {
        gl->current_texture = Texture(size);
    }

    bind_framebuffer();

    // This typically calls bind_framebuffer(), which attaches current_texture and
    // stencilbuffer to gl->framebuffer.
    Cairo::RefPtr<Cairo::Context> dummy;
    bool const painted = paint_widget(*context(), dummy);

    // --- DEBUG: save the framebuffer to PNG (set INKSCAPE_SAVE_FRAMES=1 to enable) ---
    static bool const save_frames = std::getenv("INKSCAPE_SAVE_FRAMES") != nullptr;
    if (painted && save_frames) {
        static int frame_num = 0;
        static bool dir_made = false;
        if (!dir_made) {
            mkdir("/tmp/inkscape_frames", 0755);
            dir_made = true;
        }
        ++frame_num;
        printf("  [PNG#%d] capturing...\n", frame_num);
        int const W = size.x();
        int const H = size.y();
        // Ensure gl->framebuffer is bound for read
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gl->framebuffer);
        auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, W, H);
        unsigned char *data = surface->get_data();
        int const stride = surface->get_stride();
        // Read pixels (GL gives bottom-up RGBA; we'll flip and convert below)
        std::vector<unsigned char> tmp(W * H * 4);
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data());
        // Convert RGBA bottom-up to BGRA (cairo ARGB32 on little-endian) top-down
        for (int y = 0; y < H; ++y) {
            unsigned char const *src = tmp.data() + (H - 1 - y) * W * 4;
            unsigned char *dst = data + y * stride;
            for (int x = 0; x < W; ++x) {
                dst[0] = src[2]; // B
                dst[1] = src[1]; // G
                dst[2] = src[0]; // R
                dst[3] = src[3]; // A
                src += 4;
                dst += 4;
            }
        }
        surface->mark_dirty();
        char path[128];
        snprintf(path, sizeof(path), "/tmp/inkscape_frames/frame_%05d.png", frame_num);
        surface->write_to_png(path);
        printf("  saved %s (%dx%d)\n", path, W, H);
    }

    // Composite our offscreen FBO into the QOpenGLWidget's default framebuffer.
    // (Equivalent to GTK's snapshot->append_texture() step.) Both source and destination
    // use the same bottom-left origin, so no Y flip is needed here.
    GLuint const default_fbo = defaultFramebufferObject();

    if (painted) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gl->framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, default_fbo);
        glBlitFramebuffer(0, 0, size.x(), size.y(),
                          0, 0, size.x(), size.y(),
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    } else {
        // Nothing was painted; clear the default FBO so we don't show stale content.
        glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Return the texture to the pool. Unlike GTK (where ownership is transferred to
    // a Gdk::GLTexture and returned via a destroy callback once the snapshot is consumed),
    // in Qt the blit above has already finished consuming it, so we can return it now.
    gl->spare_textures.emplace_back(std::move(gl->current_texture));

// printf("paintGL: returned texture %u to pool, pool size now %zu\n", gl->spare_textures.back().id(), gl->spare_textures.size());
}

/*
void OptGLArea::snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot)
{
    if (opengl_enabled) {
        auto const size = Geom::IntPoint(get_width(), get_height()) * get_scale_factor();

        if (size.x() == 0 || size.y() == 0) {
            return;
        }

        gl->context->make_current();

        // Check if the size has changed.
        if (size != gl->size) {
            gl->size = size;

            // Resize the framebuffer.
            glBindRenderbuffer(GL_RENDERBUFFER, gl->stencilbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x(), size.y());

            // Resize the texture builder.
            gl->builder->set_width(size.x());
            gl->builder->set_height(size.y());
        }

        // Discard wrongly-sized spare textures.
        std::erase_if(gl->spare_textures, [&] (auto &tex) { return tex.size() != size; });
        // Todo: Consider clearing out excess spare textures every once in a while.

        // Set the current texture.
        assert(!gl->current_texture);
        if (!gl->spare_textures.empty()) {
            // Grab a spare texture.
            gl->current_texture = std::move(gl->spare_textures.back());
            gl->spare_textures.pop_back();
        } else {
            // Create a new one.
            gl->current_texture = Texture(size);
        }

        // This typically calls bind_framebuffer().
        paint_widget({});

        // Wrap the OpenGL texture we've just drawn to in a Gdk::GLTexture.
        gl->builder->set_id(gl->current_texture.id());
        auto gdktexture = std::static_pointer_cast<Gdk::GLTexture>(gl->builder->build(
            share_lambda([texture = std::move(gl->current_texture),
                          context = gl->context,
                          gl_weak = weakify(gl)] () mutable
            {
                if (auto gl = gl_weak.lock()) {
                    // Return the texture to the texture pool.
                    gl->spare_textures.emplace_back(std::move(texture));
                } else {
                    // Destroy the texture in its GL context.
                    context->make_current();
                    texture.clear();
                    Gdk::GLContext::clear_current();
                }
            })
        ));

        // Render the texture upside-down.
        // Todo: The canvas does the same, so both transformations can be removed.
        snapshot->save();
        snapshot->translate({ 0.0f, (float)get_height() });
        snapshot->scale(1, -1);
        snapshot->append_texture(std::move(gdktexture), Gdk::Graphene::Rect(0, 0, get_width(), get_height()).gobj());
        snapshot->restore();
    } else {
        auto const cr = snapshot->append_cairo(Gdk::Graphene::Rect(0, 0, get_width(), get_height()).gobj());
        paint_widget(cr);
    }
}*/

} // namespace Inkscape::UI::Widget
