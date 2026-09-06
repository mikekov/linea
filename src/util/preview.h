// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Utility functions for previewing icon representation.
 */
/* Authors:
 *   Jon A. Cruz
 *   Bob Jamison
 *   Other dudes from The Inkscape Organization
 *   Abhishek Sharma
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *
 * Copyright (C) 2004 Bob Jamison
 * Copyright (C) 2005,2010 Jon A. Cruz
 * Copyright (C) 2021 Anshudhar Kumar Singh
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UTIL_PREVIEW_H
#define INKSCAPE_UTIL_PREVIEW_H

#include <cstdint>
#include <cairomm/surface.h>

#include "display/drawing.h"
#include "async/channel.h"

class SPDocument;
class SPItem;

namespace Inkscape {
namespace UI {
namespace Preview {

/**
 * Launch a background task to render a drawing to a surface.
 *
 * If the area to render is invalid, nothing is returned and no action is taken.
 * Otherwise, first the drawing is snapshotted, then an async task is launched to render the drawing to a surface.
 * Upon completion, the drawing is unsnapshotted on the calling thread and the result passed to onfinished().
 * If the return object is destroyed before this happens, then the drawing will instead be destroyed on an unspecified
 * thread while still in the snapshotted state.
 *
 * Contracts: (This isn't Rust, so we need a comment instead, and great trust in the caller.)
 *
 *    - The caller must ensure onfinished() remains valid to call during the lifetime of the return object.
 *      (This is the same as for sigc::slots and connections.)
 *
 *    - The caller must not call drawing->unsnapshot(), or any other method that bypasses snapshotting.
 *      However, it is ok to modify or destroy drawing in any other way, because the background task has shared
 *      ownership of the drawing (=> Sync), and snapshotting prevents modification of the data being read by the
 *      background task (=> Send/const).
 */
Cairo::RefPtr<Cairo::ImageSurface>
render_preview(SPDocument *doc, std::shared_ptr<Inkscape::Drawing> drawing, uint32_t bg_color, Inkscape::DrawingItem *item,
                                    unsigned width_in, unsigned height_in, Geom::Rect const &dboxIn);

/**
 * Render a page preview to a transparent surface.
 *
 * Draws the page background, content, optional border and optional drop shadow.
 * The area outside the page is left transparent so the caller can composite it
 * over any background.
 *
 * If @p max_page_rect has a non-zero area, it is used to compute a common scale
 * so all pages are rendered with the same relative size. Otherwise the page is
 * scaled to fit the requested preview size.
 */
Cairo::RefPtr<Cairo::ImageSurface>
render_page_preview(SPDocument* doc, std::shared_ptr<Inkscape::Drawing> drawing,
                    Geom::Rect const& page_rect, uint32_t page_color,
                    uint32_t border_color, uint32_t shadow_color,
                    bool draw_border, bool draw_shadow,
                    unsigned width_in, unsigned height_in,
                    double dpr = 1.0,
                    Geom::Rect const& max_page_rect = {});

} // namespace Preview
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UTIL_PREVIEW_H
