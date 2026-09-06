// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_NR_FILTER_FLOOD_H
#define SEEN_NR_FILTER_FLOOD_H

/*
 * feFlood filter primitive renderer
 *
 * Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <optional>
#include "display/nr-filter-primitive.h"

namespace Inkscape {
namespace Filters {

class FilterFlood : public FilterPrimitive
{
public:
    FilterFlood();
    ~FilterFlood() override;

    void render_cairo(FilterSlot &slot) const override;
    bool can_handle_affine(Geom::Affine const &) const override;
    double complexity(Geom::Affine const &ctm) const override;
    bool uses_background()  const override { return false; }
    
    void set_color(guint32 c);

    Glib::ustring name() const override { return Glib::ustring("Flood"); }

private:
    guint32 color;
};

} // namespace Filters
} // namespace Inkscape

#endif // SEEN_NR_FILTER_FLOOD_H
