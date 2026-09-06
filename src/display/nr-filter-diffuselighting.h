// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_NR_FILTER_DIFFUSELIGHTING_H
#define SEEN_NR_FILTER_DIFFUSELIGHTING_H

/*
 * feDiffuseLighting renderer
 *
 * Authors:
 *   Niko Kiirala <niko@kiirala.com>
 *   Jean-Rene Reinhard <jr@komite.net>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <optional>
#include "display/nr-light-types.h"
#include "display/nr-filter-primitive.h"
#include "display/nr-filter-slot.h"
#include "display/nr-filter-units.h"

class SPFeDistantLight;
class SPFePointLight;
class SPFeSpotLight;

namespace Inkscape {
namespace Filters {

class FilterDiffuseLighting : public FilterPrimitive
{
public:
    FilterDiffuseLighting();
    ~FilterDiffuseLighting() override;

    void render_cairo(FilterSlot &slot) const override;
    void area_enlarge(Geom::IntRect &area, Geom::Affine const &trans) const override;
    double complexity(Geom::Affine const &ctm) const override;

    union {
        DistantLightData distant;
        PointLightData point;
        SpotLightData spot;
    } light;
    LightType light_type;
    double diffuseConstant;
    double surfaceScale;
    guint32 lighting_color;

    Glib::ustring name() const override { return "Diffuse Lighting"; }
};

} // namespace Filters
} // namespace Inkscape

#endif // SEEN_NR_FILTER_DIFFUSELIGHTING_H
