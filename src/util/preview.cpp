// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Utility functions for generating export previews.
 */
/* Authors:
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2021 Anshudhar Kumar Singh
 *               2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "preview.h"

#include <algorithm>
#include <cmath>

#include "display/cairo-utils.h"
#include "display/drawing-context.h"
#include "ui/pixel-alignment.h"

namespace Inkscape {
namespace UI {
namespace Preview {

Cairo::RefPtr<Cairo::ImageSurface>
render_preview(SPDocument *doc, std::shared_ptr<Inkscape::Drawing> drawing, uint32_t bg,
               Inkscape::DrawingItem *item, unsigned width_in, unsigned height_in, Geom::Rect const &dboxIn)
{
    if (!drawing->root())
        return {};

    // Calculate a scaling factor for the requested bounding box.
    double sf = 1.0;
    Geom::IntRect ibox = dboxIn.roundOutwards();
    if (ibox.width() != width_in || ibox.height() != height_in) {
        // Adjust by one pixel to fit in anti-aliasing pixels
        sf = std::min((double)(width_in - 1) / dboxIn.width(),
                      (double)(height_in - 1) / dboxIn.height());
        auto scaled_box = dboxIn * Geom::Scale(sf);
        ibox = scaled_box.roundOutwards();
    }

    auto pdim = Geom::IntPoint(width_in, height_in);
    // The unsigned width/height can wrap around when negative.
    int dx = ((int)width_in - ibox.width()) / 2;
    int dy = ((int)height_in - ibox.height()) / 2;
    auto area = Geom::IntRect::from_xywh(ibox.min() - Geom::IntPoint(dx, dy), pdim);

    /* Actual renderable area */
    auto const ua = Geom::intersect(ibox, area);
    if (!ua) {
        return {};
    }
    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, ua->width(), ua->height());

    auto on_error = [&] (char const *err) {
        std::cerr << "render_preview: " << err << std::endl;
        surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, ua->width(), ua->height());
    };

    try {
        {
            auto cr = Cairo::Context::create(surface);
            cr->rectangle(0, 0, ua->width(), ua->height());

            // We always use checkerboard to indicate transparency.
            if (SP_RGBA32_A_F(bg) < 1.0) {
                auto background = ink_cairo_pattern_create_checkerboard(bg, false);
                cr->set_source(background);
                cr->fill();
            }

            // We always draw the background on top to indicate partial backgrounds.
            cr->set_source_rgba(SP_RGBA32_R_F(bg), SP_RGBA32_G_F(bg), SP_RGBA32_B_F(bg), SP_RGBA32_A_F(bg));
            cr->fill();
        }

        // Resize the contents to the available space with a scale factor.
        drawing->root()->setTransform(Geom::Scale(sf));
        drawing->update();

        auto dc = Inkscape::DrawingContext(surface->cobj(), ua->min());
        if (item) {
            // Render just one item
            item->render(dc, *ua);
        } else {
            // Render drawing.
            drawing->render(dc, *ua);
        }

        surface->flush();
    } catch (std::bad_alloc const &e) {
        on_error(e.what());
    } catch (Cairo::logic_error const &e) {
        on_error(e.what());
    }

    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface>
render_page_preview(SPDocument* doc, std::shared_ptr<Inkscape::Drawing> drawing, const Geom::Rect& page_rect,
                    uint32_t page_color, uint32_t border_color, uint32_t shadow_color,
                    bool draw_border, bool draw_shadow,
                    unsigned width_in, unsigned height_in,
                    double dpr,
                    Geom::Rect const& max_page_rect)
{
    if (!drawing || !drawing->root() || width_in < 2 || height_in < 2 || dpr <= 0) {
        return {};
    }

    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, width_in, height_in);
    auto cr = Cairo::Context::create(surface);

    double shadow_size = 8.0 * dpr;
    // keep most of the shadow visible while leaving more room for the page
    double margin = shadow_size - 2.0 * dpr;

    double avail_w = std::max(2.0, static_cast<double>(width_in) - 2 * margin);
    double avail_h = std::max(2.0, static_cast<double>(height_in) - 2 * margin);

    auto const fit_rect = (max_page_rect.width() > 0 && max_page_rect.height() > 0) ? max_page_rect : page_rect;
    double scale = std::min(avail_w / fit_rect.width(), avail_h / fit_rect.height());
    double pw = std::max(2.0, std::round(page_rect.width() * scale));
    double ph = std::max(2.0, std::round(page_rect.height() * scale));

    double ox = std::round((width_in - pw) / 2.0);
    double oy = std::round((height_in - ph) / 2.0);
    Geom::Rect pixel_rect(ox, oy, ox + pw, oy + ph);
    pixel_rect = Inkscape::pixel_align(pixel_rect, Inkscape::RectLineAlignment::CenterInside, 0, 1);

    // shadow is drawn on the transparent background
    if (draw_shadow) {
        auto const a = SP_RGBA32_A_F(shadow_color);
        ink_cairo_draw_drop_shadow(cr, pixel_rect, shadow_size, shadow_color, a);
    }

    // page background
    cr->set_source_rgba(SP_RGBA32_R_F(page_color), SP_RGBA32_G_F(page_color),
                        SP_RGBA32_B_F(page_color), SP_RGBA32_A_F(page_color));
    cr->rectangle(pixel_rect.left(), pixel_rect.top(), pixel_rect.width(), pixel_rect.height());
    cr->fill();

    // content rendered on top of the page color
    auto content = render_preview(doc, drawing, page_color, nullptr,
                                  static_cast<unsigned>(pixel_rect.width()),
                                  static_cast<unsigned>(pixel_rect.height()), page_rect);
    if (content) {
        cr->save();
        cr->rectangle(pixel_rect.left(), pixel_rect.top(), pixel_rect.width(), pixel_rect.height());
        cr->clip();
        cr->set_source(content, pixel_rect.left(), pixel_rect.top());
        cr->paint();
        cr->restore();
    }

    // page border drawn on the outside of the page, aligned to the pixel grid
    if (draw_border) {
        auto border = Inkscape::pixel_align(pixel_rect, Inkscape::RectLineAlignment::Outside, 1, 1);
        cr->rectangle(border.left(), border.top(), border.width(), border.height());
        cr->set_source_rgba(SP_RGBA32_R_F(border_color), SP_RGBA32_G_F(border_color),
                            SP_RGBA32_B_F(border_color), SP_RGBA32_A_F(border_color));
        cr->set_line_width(1.0);
        cr->stroke();
    }

    surface->flush();
    return surface;
}

} // namespace Preview
} // namespace UI
} // namespace Inkscape
