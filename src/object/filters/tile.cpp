// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feTile> implementation.
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "tile.h"

#include "display/nr-filter-tile.h"  // for FilterTile

namespace Inkscape {
class DrawingItem;
namespace Filters {
class FilterPrimitive;
} // namespace Filters
} // namespace Inkscape

std::unique_ptr<Inkscape::Filters::FilterPrimitive> SPFeTile::build_renderer(Inkscape::DrawingItem*) const
{
    auto tile = std::make_unique<Inkscape::Filters::FilterTile>();
    build_renderer_common(tile.get());
    return tile;
}
