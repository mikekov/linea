// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Jon A. Cruz <jon@joncruz.org>
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2013-2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_COMPONENTS_H
#define SEEN_COLORS_COMPONENTS_H

#include <lcms2.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "enum.h"

namespace Inkscape::Colors::Space {

enum class Type;

// Channel roles of a rectangular color plate
struct PlateAxes {
    int fixed = 0;      // channel held constant (driven by its slider)
    int x = 1;          // channel along the horizontal axis, 0 at the left
    int y = 2;          // channel along the vertical axis, 0 at the bottom
    double x_scale = 1; // fraction of the x channel's range shown, 1 = all of it
    double y_scale = 1; // fraction of the y channel's range shown, 1 = all of it
};

// How color plates present colors of a space
struct PlateSpec {
    Type polar = Type::NONE; // space of wheel/ring plates; must be HSV-like: 0 hue (angle), 1 saturation (radius), 2 value
    Type rect = Type::NONE;  // space of the rectangular plate
    PlateAxes axes;          // channel roles in the 'rect' space
    bool gamut = false;      // 'rect' is unbounded: draw it clipped to the requested gamut
    bool shared_hue = false; // this space's hue channel is the plate spaces' hue (no conversion residue)
};

enum class Traits {
    None = 0,
    Picker = 1,   // show color picker of this type in UI
    Internal = 2, // internal use only, has converters and tests, or is supported by CSS toString
    CMS = 4,      // CMS use only, no conversion math available
};
inline Traits operator & (const Traits lhs, const Traits rhs) {
    using underlying = std::underlying_type_t<Traits>;
    return static_cast<Traits>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}
inline Traits operator | (const Traits lhs, const Traits rhs) {
    using underlying = std::underlying_type_t<Traits>;
    return static_cast<Traits>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

// Unit impacts component presentation in the UI and dictates what its scale is
enum class Unit {
    EightBit,   // one byte, values from 0..255, unitless
    Percent,    // 0..100%
    Degree,     // 0..360 degree
    Linear1024, // 0..1024 for linear RGB
    Chroma40    // an oddball 0..40 for OkLch hue encoding
};

struct Component
{
    Component(Type type, unsigned int index, std::string id, std::string name, std::string tip, Unit unit = Unit::EightBit);
    Component(std::string id, std::string name, std::string tip, Unit unit = Unit::EightBit);

    Type type;
    unsigned int index;
    std::string id;
    std::string name;
    std::string tip;
    unsigned scale;
    Unit unit = Unit::EightBit;

    double normalize(double value) const;
};

class Components
{
public:
    Components() = default;
    Components(Type type, PlateSpec plate, Traits traits, std::vector<Component> components):
        _type(type), _components(std::move(components)), _plate(plate), _traits(traits) {}

    static Components const &get(Type type, bool alpha = false);

    const std::vector<Component>& getAll() const { return _components; }

    std::vector<Component>::const_iterator begin() const { return std::begin(_components); }
    std::vector<Component>::const_iterator end() const { return std::end(_components); }
    Component const &operator[](const unsigned int index) const { return _components[index]; }

    Type getType() const { return _type; }
    unsigned size() const { return _components.size(); }

    void add(std::string id, std::string name, std::string tip, Unit unit = Unit::EightBit);
    void setType(Type type) { _type = type; }
    // How color plates present colors of this space; Type::NONE when unsupported
    const PlateSpec& plate() const { return _plate; }
    // Index of the first component measured in degrees (the hue channel), if any
    std::optional<unsigned> hue_index() const;
    // Trait(s) of those components
    Traits traits() const { return _traits; }

private:
    Type _type = Type::NONE;
    std::vector<Component> _components;
    PlateSpec _plate;
    Traits _traits = Traits::None;
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_COMPONENTS_H
