// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Swatch — a document color swatch bound to a CSS custom property.
 */

#ifndef LINEA_SWATCH_TYPE_H
#define LINEA_SWATCH_TYPE_H

#include <string>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include "colors/color.h"
#include "style-helpers.h"

namespace Linea {

// A swatch is a color bound to a document CSS custom property. It carries the
// full CSS name (with "--" prefix) for stylesheet edits and var() references,
// and a prefix-free display name for user-facing presentation.
class Swatch {
public:
    Swatch(Inkscape::Css::CustomCSSProperty property, Inkscape::Colors::Color color);

    // Custom property name including the "--" prefix; for stylesheet edits.
    const std::string& cssName() const { return _property.name; }

    // CSS var() function call for this swatch.
    std::string cssVar() const { return Inkscape::Css::css_var(_property.name); }

    // Prefix-free name for display.
    std::string displayName() const { return _property.display_name(); }

    const Inkscape::Colors::Color& color() const { return _color; }
    void setColor(Inkscape::Colors::Color color) { _color = std::move(color); }

    // Attempt to rename the underlying custom property; returns the new name if successful, empty string otherwise.
    std::string tryRename(std::string name);

    void setName(std::string name) { _property.name = std::move(name); }

private:
    Inkscape::Css::CustomCSSProperty _property;
    Inkscape::Colors::Color _color;
};

// All color-valued properties as swatches, in order. When name_colors is
// true, each parsed color is named after its property.
std::vector<Swatch> swatches_from_properties(const std::vector<Inkscape::Css::CustomCSSProperty>& properties,
                                             bool name_colors = false);

struct swatch_by_name {};

// A document's swatches: index 0 preserves stylesheet declaration order,
// index swatch_by_name gives map-like lookup by custom property name.
using SwatchSet = boost::multi_index_container<
    Swatch,
    boost::multi_index::indexed_by<
        boost::multi_index::sequenced<>,
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<swatch_by_name>,
            boost::multi_index::const_mem_fun<Swatch, const std::string&, &Swatch::cssName>>
    >>;

} // namespace Linea

#endif // LINEA_SWATCH_TYPE_H
