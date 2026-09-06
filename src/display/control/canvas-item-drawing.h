// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * A class to render the SVG drawing.
 */
/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of _SPCanvasArena.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_CANVAS_ITEM_DRAWING_H
#define SEEN_CANVAS_ITEM_DRAWING_H

#include <memory>
#include <mutex>
#include <sigc++/signal.h>
#include <cairomm/refptr.h>
#include <cairomm/region.h>
#include <cairomm/surface.h>

#include "canvas-item.h"

namespace Inkscape {

class Drawing;
class DrawingItem;
class Updatecontext;

class CanvasItemDrawing final : public CanvasItem
{
public:
    CanvasItemDrawing(CanvasItemGroup *group);

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Display
    Inkscape::Drawing *get_drawing() { return _drawing.get(); }

    // Drawing items
    void set_active(Inkscape::DrawingItem *active) { _active_item = active; }
    Inkscape::DrawingItem *get_active() { return _active_item; }

    // Events
    bool handle_event(CanvasEvent const &event) override;
    void set_sticky(bool sticky) { _sticky = sticky; }
    void set_pick_outline(bool pick_outline) { _pick_outline = pick_outline; }

    // Invalidation (dirty rect in drawing coordinates).
    void redraw_drawing_area(const Geom::IntRect& area);

    // Pixel preview
    void set_pixel_preview(bool enabled);
    void set_pixel_preview_cap(double cap);
    bool get_pixel_preview() const { return _pixel_preview; }
    double get_pixel_preview_cap() const { return _pixel_preview_cap; }

    // Signals
    sigc::connection connect_drawing_event(sigc::slot<bool(CanvasEvent const &, Inkscape::DrawingItem *)> slot) {
        return _drawing_event_signal.connect(slot);
    }

protected:
    ~CanvasItemDrawing() override;

    void _update(bool propagate) override;
    void _render(Inkscape::CanvasItemBuffer &buf) const override;

    // Selection
    Geom::Point _c;
    double _delta = Geom::infinity();
    Inkscape::DrawingItem *_active_item = nullptr;
    Inkscape::DrawingItem *_picked_item = nullptr;

    // Display
    std::unique_ptr<Inkscape::Drawing> _drawing;
    Geom::Affine _drawing_affine;

    // Pixel preview: drawing is rendered at a capped resolution and magnified.
    void _ensure_preview_store(const Geom::IntRect& needed) const;
    bool _pixel_preview = false;
    double _pixel_preview_cap = 1.0; // Max resolution in canvas px per document px (1.0 = 100%).
    double _preview_scale = 1.0; // Canvas -> drawing coordinate scale (<= 1).
    mutable std::mutex _preview_mutex;
    mutable Cairo::RefPtr<Cairo::ImageSurface> _preview_surface;
    mutable Geom::IntRect _preview_rect;
    mutable Cairo::RefPtr<Cairo::Region> _preview_clean;

    // Events
    bool _cursor = false;
    bool _sticky = false; // Pick anything, even if hidden.
    bool _pick_outline = false;

    // Signals
    sigc::signal<bool(CanvasEvent const &, Inkscape::DrawingItem *)> _drawing_event_signal;
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_DRAWING_H
