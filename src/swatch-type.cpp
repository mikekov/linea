// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Swatch — a document color swatch bound to a CSS custom property.
 */

#include "swatch-type.h"

namespace Linea {

Swatch::Swatch(Inkscape::Css::CustomCSSProperty property, Inkscape::Colors::Color color)
    : _property(std::move(property))
    , _color(std::move(color)) {}

std::string Swatch::tryRename(std::string name) {
    const auto newName = Inkscape::Css::normalize_custom_property_name(name);
    if (!newName) {
        return {};
    }
    return *newName;
}

std::vector<Swatch> swatches_from_properties(const std::vector<Inkscape::Css::CustomCSSProperty>& properties,
                                             bool name_colors) {
    std::vector<Swatch> swatches;
    for (auto& prop : properties) {
        if (auto color = Inkscape::Colors::Color::parse(prop.value)) {
            if (name_colors) {
                color->setName(prop.name);
            }
            swatches.emplace_back(Swatch{prop, std::move(*color)});
        }
        // Non-color properties stay in the stylesheet but are not swatches.
    }
    return swatches;
}

} // namespace Linea
