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

#include "canvas-item-drawing.h"

#include "desktop.h"

#include "display/drawing.h"
#include "display/drawing-context.h"
#include "display/drawing-item.h"
#include "display/drawing-group.h"

#include "helper/geom.h"
#include "ui/util.h"
#include "ui/widget/canvas.h"
#include "ui/widget/events/canvas-event.h"
#include "ui/modifiers.h"

namespace Inkscape {

/**
 * Create the drawing. One per window!
 */
CanvasItemDrawing::CanvasItemDrawing(CanvasItemGroup *group)
    : CanvasItem(group)
    , _drawing{std::make_unique<Drawing>(this)}
{
    _name = "CanvasItemDrawing";
    _pickable = true;

    auto root = new DrawingGroup(*_drawing);
    root->setPickChildren(true);
    _drawing->setRoot(root);
}

CanvasItemDrawing::~CanvasItemDrawing() = default;

/**
 * Returns true if point p (in canvas units) is inside some object in drawing.
 */
bool CanvasItemDrawing::contains(Geom::Point const &p, double tolerance)
{
    if (tolerance != 0) {
        std::cerr << "CanvasItemDrawing::contains: Non-zero tolerance not implemented!" << std::endl;
    }

    _picked_item = _drawing->pick(p, _drawing->cursorTolerance(), _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);

    if (_picked_item) {
        // This will trigger a signal that is handled by our event handler. Seems a bit of a
        // round-about way of doing things but it matches what other pickable canvas-item classes do.
        return true;
    }

    return false;
}

/**
 * Update and redraw drawing.
 */
void CanvasItemDrawing::_update(bool)
{
    // Undo y-axis flip. This should not be here!!!!
    auto new_drawing_affine = affine();
    if (auto desktop = get_canvas()->get_desktop()) {
        new_drawing_affine = desktop->doc2dt() * new_drawing_affine;
    }

    // Pixel preview: cap the drawing resolution.
    if (_pixel_preview) {
        double const zoom = new_drawing_affine.descrim();
        _preview_scale = zoom > _pixel_preview_cap ? _pixel_preview_cap / zoom : 1.0;
        new_drawing_affine = new_drawing_affine * Geom::Scale(_preview_scale);
    } else {
        _preview_scale = 1.0;
    }
    _drawing->setCanvasScale(_preview_scale);

    bool affine_changed = _drawing_affine != new_drawing_affine;
    if (affine_changed) {
        _drawing_affine = new_drawing_affine;
        auto lock = std::lock_guard(_preview_mutex);
        _preview_surface.reset();
        _preview_clean.reset();
    }

    _drawing->update(Geom::IntRect::infinite(), _drawing_affine, DrawingItem::STATE_ALL, affine_changed * DrawingItem::STATE_ALL);

    _bounds = expandedBy(_drawing->root()->drawbox(), 1); // Avoid aliasing artifacts
    if (_bounds && _preview_scale != 1.0) {
        *_bounds *= Geom::Scale(1.0 / _preview_scale);
    }

    if (_cursor) {
        /* Mess with enter/leave notifiers */
        auto new_drawing_item = _drawing->pick(_c, _delta, _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);
        if (_active_item != new_drawing_item) {
            // Fixme: These crossing events have no modifier state set.

            if (_active_item) {
                auto event = LeaveEvent();
                _drawing_event_signal.emit(event, _active_item);
            }

            _active_item = new_drawing_item;

            if (_active_item) {
                auto event = EnterEvent();
                event.pos = _c;
                _drawing_event_signal.emit(event, _active_item);
            }
        }
    }
}

/**
 * Render drawing to screen via Cairo.
 */
void CanvasItemDrawing::_render(Inkscape::CanvasItemBuffer &buf) const
{
    if (_preview_scale == 1.0 || buf.outline_pass) {
        auto dc = Inkscape::DrawingContext(buf.cr->cobj(), buf.rect.min());
        _drawing->render(dc, buf.rect, buf.outline_pass * DrawingItem::RENDER_OUTLINE);
        return;
    }

    // Pixel preview: render the drawing at the capped resolution into a shared store, then magnify.
    auto const k = _preview_scale;
    auto const needed = (Geom::Rect(buf.rect) * Geom::Scale(k)).roundOutwards();

    auto lock = std::lock_guard(_preview_mutex);
    _ensure_preview_store(needed);

    buf.cr->save();
    buf.cr->rectangle(0, 0, buf.rect.width(), buf.rect.height());
    buf.cr->clip();
    buf.cr->translate(-buf.rect.left(), -buf.rect.top());
    buf.cr->scale(1.0 / k, 1.0 / k);
    buf.cr->set_source(_preview_surface, _preview_rect.left(), _preview_rect.top());
    cairo_pattern_set_filter(cairo_get_source(buf.cr->cobj()), CAIRO_FILTER_NEAREST);
    cairo_pattern_set_extend(cairo_get_source(buf.cr->cobj()), CAIRO_EXTEND_PAD);
    buf.cr->paint();
    buf.cr->restore();
}

/**
 * Ensure the pixel preview store covers the given rectangle (in drawing coordinates) with clean content.
 * Must be called with _preview_mutex held.
 */
void CanvasItemDrawing::_ensure_preview_store(const Geom::IntRect& needed) const {
    if (!_preview_surface || !_preview_rect.contains(needed)) {
        constexpr int margin = 64;
        constexpr int64_t max_area = int64_t(1) << 24;
        auto rect = expandedBy(needed, margin);
        bool keep_old = false;
        if (_preview_surface) {
            auto grown = rect;
            grown.unionWith(_preview_rect);
            if (int64_t(grown.width()) * grown.height() <= max_area) {
                rect = grown;
                keep_old = true;
            }
        }
        auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, rect.width(), rect.height());
        if (keep_old) {
            auto cr = Cairo::Context::create(surface);
            cr->set_source(_preview_surface, _preview_rect.left() - rect.left(), _preview_rect.top() - rect.top());
            cr->paint();
        } else {
            _preview_clean.reset();
        }
        _preview_surface = std::move(surface);
        _preview_rect = rect;
        if (_preview_clean) {
            _preview_clean->intersect(geom_to_cairo(rect));
        }
    }

    if (!_preview_clean) {
        _preview_clean = Cairo::Region::create();
    }

    auto dirty = Cairo::Region::create(geom_to_cairo(needed));
    dirty->subtract(_preview_clean);

    for (int i = 0; i < dirty->get_num_rectangles(); i++) {
        auto const rect = cairo_to_geom(dirty->get_rectangle(i));
        auto dc = Inkscape::DrawingContext(_preview_surface->cobj(), _preview_rect.min());
        dc.save();
        dc.rectangle(rect);
        dc.clip();
        dc.setOperator(CAIRO_OPERATOR_CLEAR);
        dc.paint();
        dc.setOperator(CAIRO_OPERATOR_OVER);
        _drawing->render(dc, rect);
        dc.restore();
    }

    _preview_clean->do_union(dirty);
}

/**
 * Mark an area of the drawing (in drawing coordinates) as needing redrawing.
 */
void CanvasItemDrawing::redraw_drawing_area(const Geom::IntRect& area) {
    if (_preview_scale == 1.0) {
        get_canvas()->redraw_area(Geom::Rect(area));
        return;
    }

    {
        auto lock = std::lock_guard(_preview_mutex);
        if (_preview_clean) {
            _preview_clean->subtract(geom_to_cairo(area));
        }
    }
    get_canvas()->redraw_area(Geom::Rect(area) * Geom::Scale(1.0 / _preview_scale));
}

/**
 * Enable/disable pixel preview mode, in which the drawing is rendered at a capped resolution.
 */
void CanvasItemDrawing::set_pixel_preview(bool enabled) {
    if (_pixel_preview == enabled) return;

    _pixel_preview = enabled;
    request_update();
    get_canvas()->redraw_all();
}

/**
 * Set the pixel preview resolution cap in canvas px per document px (e.g. 1.0 = 100%, 2.0 = 200%).
 */
void CanvasItemDrawing::set_pixel_preview_cap(double cap) {
    if (cap <= 0 || _pixel_preview_cap == cap) return;

    _pixel_preview_cap = cap;
    if (!_pixel_preview) return;

    request_update();
    get_canvas()->redraw_all();
}

/**
 * Handle events directed at the drawing. We first attempt to handle them here.
 */
bool CanvasItemDrawing::handle_event(CanvasEvent const &event)
{
    bool retval = false;

    inspect_event(event,
        [&] (EnterEvent const &event) {
            if (!_cursor) {
                if (_active_item) {
                    // Fixme: This warning seems to fire a lot.
                    std::cerr << "CanvasItemDrawing::event_handler: cursor entered drawing with an active item!" << std::endl;
                }
                _cursor = true;

                /* TODO ... event -> arena transform? */
                _c = event.pos;

                _active_item = _drawing->pick(_c, _drawing->cursorTolerance(), _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);
                retval = _drawing_event_signal.emit(event, _active_item);
            }
        },

        [&] (LeaveEvent const &event) {
            if (_cursor) {
                retval = _drawing_event_signal.emit(event, _active_item);
                _active_item = nullptr;
                _cursor = false;
            }
        },

        [&] (MotionEvent const &event) {
            /* TODO ... event -> arena transform? */
            _c = event.pos;

            auto new_drawing_item = _drawing->pick(_c, _drawing->cursorTolerance(), _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);
            if (_active_item != new_drawing_item) {

                /* fixme: What is wrong? */
                if (_active_item) {
                    auto event2 = LeaveEvent();
                    event2.modifiers = event.modifiers;
                    retval = _drawing_event_signal.emit(event2, _active_item);
                }

                _active_item = new_drawing_item;

                if (_active_item) {
                    auto event2 = EnterEvent();
                    event2.modifiers = event.modifiers;
                    event2.pos = event.pos;
                    retval = _drawing_event_signal.emit(event2, _active_item);
                }
            }
            retval = retval || _drawing_event_signal.emit(event, _active_item);
        },

        [&] (ScrollEvent const &event) {
            if (Modifiers::Modifier::get(Modifiers::Type::CANVAS_ZOOM)->active(event.modifiers)) {
                /* Zoom is emitted by the canvas as well, ignore here */
                retval = false;
                return;
            }
            retval = _drawing_event_signal.emit(event, _active_item);
        },

        [&] (CanvasEvent const &event) {
            // Just send event.
            retval = _drawing_event_signal.emit(event, _active_item);
        }
    );

    return retval;
}

} // namespace Inkscape
