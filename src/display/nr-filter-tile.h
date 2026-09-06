// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_NR_FILTER_TILE_H
#define SEEN_NR_FILTER_TILE_H

/*
 * feTile filter primitive renderer
 *
 * Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "display/nr-filter-primitive.h"

namespace Inkscape {
namespace Filters {

class FilterSlot;

class FilterTile : public FilterPrimitive
{
public:
    FilterTile();
    ~FilterTile() override;

    void render_cairo(FilterSlot &slot) const override;
    void area_enlarge(Geom::IntRect &area, Geom::Affine const &trans) const override;
    double complexity(Geom::Affine const &ctm) const override;

    Glib::ustring name() const override { return Glib::ustring("Tile"); }
};

} // namespace Filters
} // namespace Inkscape

#endif // SEEN_NR_FILTER_TILE_H
