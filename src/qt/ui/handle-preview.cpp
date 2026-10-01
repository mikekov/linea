// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Render a preview strip of canvas control handles.
 *
 * Ported from GTK's ui/widget/handle-preview.cpp.
 */

#include "handle-preview.h"

#include <array>
#include <cmath>

#include <cairo.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>

#include <2geom/point.h>
#include <2geom/rect.h>

#include "display/control/canvas-item-buffer.h"
#include "display/control/canvas-item-ctrl.h"
#include "display/control/canvas-item-enums.h"
#include "display/control/canvas-item-group.h"
#include "ui/util.h"
#include "ui/widget/canvas.h"

namespace Linea::UI {

// Renders sample handles into a Cairo surface using an offscreen Canvas.
// The canvas is never shown, so it stays inactive and all its redraw paths
// (schedule_redraw, redraw_area, GL init) are no-ops.
QPixmap draw_handles_preview(double device_scale) {
    constexpr int step = 28; // selected to make handles fit at highest size
    constexpr auto types = std::to_array({
        Inkscape::CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW,
        Inkscape::CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE,
        Inkscape::CANVAS_ITEM_CTRL_TYPE_POINTER, // pointy, triangular handle
        Inkscape::CANVAS_ITEM_CTRL_TYPE_MARKER, // X mark
        Inkscape::CANVAS_ITEM_CTRL_TYPE_NODE_AUTO,
        Inkscape::CANVAS_ITEM_CTRL_TYPE_NODE_CUSP,
        Inkscape::CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH,
    });
    auto h = step;
    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32,
                                               (types.size() + 1) * step * device_scale,
                                               h * device_scale);
    cairo_surface_set_device_scale(surface->cobj(), device_scale, device_scale);
    auto buf = Inkscape::CanvasItemBuffer{
        .rect = Geom::IntRect(0, 0, surface->get_width(), surface->get_height()),
        .device_scale = device_scale,
        .cr = Cairo::Context::create(surface),
        .outline_pass = false
    };

// buf.cr->set_source_rgb(1.0, 0.9, 0.9); // very light red background
// buf.cr->paint();

    auto canvas = std::make_unique<Inkscape::UI::Widget::Canvas>();
    auto root = canvas->get_canvas_item_root();

    int i = 1;
    for (auto type : types) {
        auto position = Geom::Point{static_cast<Geom::Coord>(step * i++), h / 2.0};
        auto handle = new Inkscape::CanvasItemCtrl(root, type, position);

        if (type == Inkscape::CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW) handle->set_hover();
        if (type == Inkscape::CANVAS_ITEM_CTRL_TYPE_NODE_CUSP ||
            type == Inkscape::CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH) handle->set_selected();
        if (type == Inkscape::CANVAS_ITEM_CTRL_TYPE_POINTER) handle->set_angle(M_PI);

        handle->set_size(Inkscape::HandleSize::NORMAL);
    }

    root->update(true);
    root->render(buf);
    surface->flush();

    return QPixmap::fromImage(Inkscape::UI::cairo_surface_to_qimage(surface->cobj()));
}

} // namespace Linea::UI
