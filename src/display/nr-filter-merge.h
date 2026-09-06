// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_NR_FILTER_MERGE_H
#define SEEN_NR_FILTER_MERGE_H

/*
 * feMerge filter effect renderer
 *
 * Authors:
 *   Niko Kiirala <niko@kiirala.com>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>
#include "display/nr-filter-primitive.h"

namespace Inkscape {
namespace Filters {

class FilterMerge : public FilterPrimitive
{
public:
    FilterMerge();

    void render_cairo(FilterSlot &) const override;
    bool can_handle_affine(Geom::Affine const &) const override;
    double complexity(Geom::Affine const &ctm) const override;
    bool uses_background() const override;

    void set_input(int input) override;
    void set_input(int input, int slot) override;

    Glib::ustring name() const override { return Glib::ustring("Merge"); }

private:
    std::vector<int> _input_image;
};

} // namespace Filters
} // namespace Inkscape

#endif // SEEN_NR_FILTER_MERGE_H
