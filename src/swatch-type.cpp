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

} // namespace Linea
