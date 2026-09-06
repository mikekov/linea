// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#ifndef LINEA_ELEMENT_PROPERTIES_H
#define LINEA_ELEMENT_PROPERTIES_H

#include <ranges>
#include <QString>

#include "mixed-property.h"

class SPItem;

// This file defines helpers for querying element-specific geometry properties
// from either a range of SPItem* or a single SPItem*.
// Only geometric/shape attributes are handled here — style properties (fill,
// stroke, opacity, etc.) live in style-utils.h / PresentationState.
// The function detects uniform vs. mixed values and reports them via
// mixed_property<T>.

namespace Linea {

// Resolved element-specific property values across one or more items.
// Fields are added per element type (rect, ellipse, star, spiral, image, …).
// For mixed properties the stored value is the first encountered value.
struct ElementState {
    // TODO: add per-element fields, e.g.:
    // mixed_property<int> rectangles;
    mixed_property<double> rect_rx;
    mixed_property<double> rect_ry;
    mixed_property<bool> rect_round_corners;

    // mixed_property<int> ellipses;
    mixed_property<double> ellipse_cx;
    mixed_property<double> ellipse_cy;
    mixed_property<double> ellipse_rx;
    mixed_property<double> ellipse_ry;
    mixed_property<double> ellipse_start_angle;
    mixed_property<double> ellipse_end_angle;

    // mixed_property<int> stars; // stars
    // mixed_property<int> polygons;
    mixed_property<int> star_sides;
    mixed_property<bool> star_flatsided; // true -> polygon
    mixed_property<double> star_r1;
    mixed_property<double> star_r2;
    mixed_property<double> star_rounded;
    mixed_property<double> star_randomized;

    //   mixed_property<double> image_x;
    //   mixed_property<double> image_y;
    //   mixed_property<double> image_width;
    //   mixed_property<double> image_height;

    // generic
    mixed_property<QString> title;
    mixed_property<QString> description;
    // note: ID and label are not included here, they are for single selection only
    mixed_property<bool> locked;

    struct {
        int items = 0;
        int rectangles = 0;
        int ellipses = 0;
        int stars = 0;
        int polygons = 0;
        int paths = 0;
        int lines = 0;
        int groups = 0;
        int layers = 0;
        int images = 0;
        int textual = 0; // any of textual elements
        int flowtext = 0;
    } count;
};

namespace detail {
// Implemented in element-properties.cpp.
// Merges element-specific properties of a single item into props.
void merge_item_properties(ElementState& props, SPItem* item);
} // namespace detail

// Query element properties from a single item.
inline ElementState query_element_properties(SPItem* item) {
    ElementState props;
    if (item) {
        detail::merge_item_properties(props, item);
    }
    return props;
}

// Query element properties from any range of SPItem*
// (e.g. ObjectSet::items(), a vector, a span).
template<std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, SPItem*>
ElementState query_element_properties(Range&& items) {
    ElementState props;
    for (SPItem* item : items) {
        detail::merge_item_properties(props, item);
    }
    return props;
}

} // namespace Linea

#endif // LINEA_ELEMENT_PROPERTIES_H
