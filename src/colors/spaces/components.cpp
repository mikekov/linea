// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Meta data about color channels and how they are presented to users.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "components.h"

#include <algorithm>
#include <cmath>
#include <libintl.h> // avoid glib include
#include <map>
// #include <utility>
#include <glib/gi18n.h>
#include <glibmm/value.h>

#include "enum.h"

namespace Inkscape::Colors::Space {

static const std::vector<Components> get_color_spaces() {

static const std::vector<Components> color_spaces = {
    {
        Type::RGB, {.polar=Type::HSV, .rect=Type::HSV, .axes={0, 1, 2}}, Traits::Picker,
        {
            { "r", _("&R"), _("Red"), Unit::EightBit },
            { "g", _("&G"), _("Green"), Unit::EightBit },
            { "b", _("&B"), _("Blue"), Unit::EightBit }
        }
    },
    {
        Type::linearRGB, {}, Traits::Internal,
        {
            { "r", _("<sub>l</sub>&R"), _("Linear Red"), Unit::EightBit },
            { "g", _("<sub>l</sub>&G"), _("Linear Green"), Unit::EightBit },
            { "b", _("<sub>l</sub>&B"), _("Linear Blue"), Unit::EightBit }
        }
    },
    {
        Type::HSL, {.polar=Type::HSV, .rect=Type::HSV, .axes={0, 1, 2}, .shared_hue=true}, Traits::Picker,
        {
            { "h", _("&H"), _("Hue"), Unit::Degree },
            { "s", _("&S"), _("Saturation"), Unit::Percent },
            { "l", _("&L"), _("Lightness"), Unit::Percent }
        }
    },
    {
        Type::HSV, {.polar=Type::HSV, .rect=Type::HSV, .axes={0, 1, 2}, .shared_hue=true}, Traits::Picker,
        {
            { "h", _("&H"), _("Hue"), Unit::Degree },
            { "s", _("&S"), _("Saturation"), Unit::Percent },
            { "v", _("&V"), _("Value"), Unit::Percent }
        }
    },
    {
        Type::CMYK, {.polar=Type::HSV, .rect=Type::HSV, .axes={0, 1, 2}}, Traits::Internal,
        {
            { "c", _("&C"), C_("CMYK", "Cyan"), Unit::Percent },
            { "m", _("&M"), C_("CMYK", "Magenta"), Unit::Percent },
            { "y", _("&Y"), C_("CMYK", "Yellow"), Unit::Percent },
            { "k", _("&K"), C_("CMYK", "Black"), Unit::Percent }
        }
    },
    {
        Type::CMY, {}, Traits::Internal,
        {
            { "c", _("&C"), C_("CMYK", "Cyan"), Unit::Percent },
            { "m", _("&M"), C_("CMYK", "Magenta"), Unit::Percent },
            { "y", _("&Y"), C_("CMYK", "Yellow"), Unit::Percent },
        }
    },
    {
        Type::HSLUV, {.polar=Type::HSV, .rect=Type::HSV, .axes={0, 1, 2}}, Traits::Internal,
        {
            { "h", _("&H*"), _("Hue"), Unit::Degree },
            { "s", _("&S*"), _("Saturation"), Unit::Percent },
            { "l", _("&L*"), _("Lightness"), Unit::Percent }
        }
    },
    {
        Type::OKHSL, {.polar=Type::OKHSV, .rect=Type::OKHSV, .axes={0, 1, 2}, .shared_hue=true}, Traits::Picker,
        {
            { "h", _("&H<sub>ok</sub>"), _("Hue"), Unit::Degree },
            { "s", _("&S<sub>ok</sub>"), _("Saturation"), Unit::Percent },
            { "l", _("&L<sub>ok</sub>"), _("Lightness"), Unit::Percent }
        }
    },
    {
        Type::OKHSV, {}, Traits::Internal,
        {
            { "h", _("&H<sub>ok</sub>"), _("Hue"), Unit::Degree },
            { "s", _("&S<sub>ok</sub>"), _("Saturation"), Unit::Percent },
            { "v", _("&V<sub>ok</sub>"), _("Value"), Unit::Percent }
        }
    },
    {
        Type::LCH, {}, Traits::Internal,
        {
            { "l", _("&L"), _("Luminance"), Unit::EightBit },
            { "c", _("&C"), _("Chroma"), Unit::EightBit },
            { "h", _("&H"), _("Hue"), Unit::Degree },
        }
    },
    {
        Type::LUV, {}, Traits::Internal,
        {
            { "l", _("&L"), _("Luminance"), Unit::Percent },
            { "u", _("&U"), _("Chroma U"), Unit::Percent },
            { "v", _("&V"), _("Chroma V"), Unit::Percent },
        }
    },
    {
        Type::OKLAB, {}, Traits::Internal,
        {
            { "l", _("&L<sub>ok</sub>"), _("Lightness"), Unit::Percent },
            { "a", _("&A<sub>ok</sub>"), _("Component A"), Unit::Percent },
            { "b", _("&B<sub>ok</sub>"), _("Component B"), Unit::Percent }
        }
    },
    {
        Type::OKLCH, {.polar=Type::OKHSV, .rect=Type::OKLCH,
            // the axes can be any permutation of L(0)/C(1)/H(2)
            // sRGB chroma tops out near 0.32 (normalized ~0.8) — show 80% of
            // the channel range; wider gamuts pin the excess to the top edge
            .axes={.fixed=2, .x=0, .y=1, .y_scale=0.4}, .gamut=true, .shared_hue=true}, Traits::Picker,
        {
            { "l", _("&L<sub>ok</sub>"), _("Lightness"), Unit::Percent },
            { "c", _("&C<sub>ok</sub>"), _("Chroma"), Unit::Chroma40 }, // 100% is 0.4
            { "h", _("&H<sub>ok</sub>"), _("Hue"), Unit::Degree }
        }
    },
    {
        Type::LAB, {}, Traits::Internal,
        {
            { "l", _("&L"), _("Lightness"), Unit::Percent },
            { "a", _("&A"), _("Component A"), Unit::EightBit },
            { "b", _("&B"), _("Component B"), Unit::EightBit }
        }
    },
    {
        Type::YCbCr, {}, Traits::CMS,
        {
            { "y", _("&Y"), _("Y"), Unit::EightBit },
            { "cb", _("C&r"), _("Cb"), Unit::EightBit },
            { "cr", _("C&b"), _("Cr"), Unit::EightBit }
        }
    },
    {
        Type::XYZ, {}, Traits::Internal,
        {
            { "x", "&X", "X", Unit::EightBit },
            { "y", "&Y", "Y", Unit::EightBit },
            { "z", "&Z", "Z", Unit::EightBit }
        }
    },
    {
        Type::XYZ50, {}, Traits::Internal,
        {
            { "x", "&X", "X", Unit::EightBit },
            { "y", "&Y", "Y", Unit::EightBit },
            { "z", "&Z", "Z", Unit::EightBit }
        }
    },
    {
        Type::YXY, {}, Traits::Internal,
        {
            { "y1", "&Y", "Y", Unit::EightBit },
            { "x", "&x", "x", Unit::EightBit },
            { "y2", "y", "y", Unit::EightBit }
        }
    },
    {
        Type::Gray, {}, Traits::Internal,
        {
            { "gray", _("G"), _("Gray"), Unit::Linear1024 }
        }
    },
    {
        Type::Alpha, {}, Traits::Internal,
        {
        }
    }
};
    return color_spaces;
}


Component::Component(Type type, unsigned int index, std::string id, std::string name, std::string tip, Unit unit)
    : type(type)
    , index(index)
    , id(std::move(id))
    , name(std::move(name))
    , tip(std::move(tip))
    , unit(unit) {

    switch (unit) {
    case Unit::EightBit:
        scale = 255;
        break;
    case Unit::Degree:
        scale = 360;
        break;
    case Unit::Percent:
        scale = 100;
        break;
    case Unit::Linear1024:
        scale = 1024;
        break;
    case Unit::Chroma40:
        scale = 40;
        break;
    default:
        throw std::logic_error("Missing case statement in Component ctor.");
    }
}

Component::Component(std::string id, std::string name, std::string tip, Unit unit)
    : Component(Type::NONE, -1, id, name, tip, unit)
{}

/**
 * Clamp the value to between 0.0 and 1.0, except for hue which is wrapped around.
 */
double Component::normalize(double value) const
{
    if (unit == Unit::Degree && (value < 0.0 || value > 1.0)) {
        return value - std::floor(value);
    }
    return std::clamp(value, 0.0, 1.0);
}

void Components::add(std::string id, std::string name, std::string tip, Unit unit)
{
    _components.emplace_back(_type, _components.size(), std::move(id), std::move(name), std::move(tip), unit);
}

std::map<Type, Components> _build(bool alpha)
{
    std::map<Type, Components> sets;
    for (auto& components : get_color_spaces()) {
        unsigned int index = 0;
        for (auto& component : components.getAll()) {
            // patch components
            auto& rw = const_cast<Component&>(component);
            rw.type = components.getType();
            rw.index = index++;
        }
        sets[components.getType()] = components;
    }

    if (alpha) {
        for (auto &[key, val] : sets) {
            // alpha component with unique ID, so it doesn't clash with "a" in Lab
            val.add("alpha", C_("Transparency (alpha)", "&A"), _("Alpha"), Unit::Percent);
        }
    }
    return sets;
}

Components const &Components::get(Type space, bool alpha)
{
    static std::map<Type, Components> sets_no_alpha = _build(false);
    static std::map<Type, Components> sets_with_alpha = _build(true);

    auto &lookup_set = alpha ? sets_with_alpha : sets_no_alpha;
    if (auto search = lookup_set.find(space); search != lookup_set.end()) {
        return search->second;
    }
    return lookup_set[Type::NONE];
}

std::optional<unsigned> Components::hue_index() const {
    for (unsigned i = 0; i < _components.size(); ++i) {
        if (_components[i].unit == Unit::Degree) return i;
    }
    return std::nullopt;
}

} // namespace Inkscape::Colors::Space
