// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Swatch — a document color swatch bound to a CSS custom property.
 */

#ifndef LINEA_SWATCH_TYPE_H
#define LINEA_SWATCH_TYPE_H

#include <string>

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
    std::string cssVar() const { return "var(" + _property.name + ")"; }

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

} // namespace Linea

#endif // LINEA_SWATCH_TYPE_H
