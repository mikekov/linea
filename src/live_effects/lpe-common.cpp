// SPDX-License-Identifier: GPL-2.0-or-later

#include "lpe-common.h"

#include <glibmm/i18n.h>

#include "object/sp-item-group.h"
#include "object/sp-lpe-item.h"
#include "object/sp-path.h"
#include "object/sp-shape.h"
#include "preferences.h"
#include "util/cast.h"

namespace {

constexpr auto favs_path = "/dialogs/livepatheffect/favs";

bool sp_has_fav(const Glib::ustring& effect) {
    Inkscape::Preferences* prefs = Inkscape::Preferences::get();
    Glib::ustring favlist = prefs->getString(favs_path);
    return favlist.find(effect) != favlist.npos;
}

Glib::ustring get_tooltip(const Inkscape::LivePathEffect::EffectType type, const Glib::ustring& untranslated_label) {
    const auto& converter = Inkscape::LivePathEffect::LPETypeConverter;
    Glib::ustring tooltip = _(converter.get_description(type).c_str());
    if (tooltip != untranslated_label) {
        // TRANSLATORS: %1 is the untranslated label. %2 is the effect type description.
        tooltip = Glib::ustring::compose("[%1] %2", untranslated_label, tooltip);
    }
    return tooltip;
}

bool can_apply(const Inkscape::LivePathEffect::EnumEffectDataConverter<Inkscape::LivePathEffect::EffectType>& converter,
               Inkscape::LivePathEffect::EffectType etype, const Glib::ustring& item_type, const bool has_clip,
               const bool has_mask) {
    if (!has_clip && etype == Inkscape::LivePathEffect::POWERCLIP) {
        return false;
    }

    if (!has_mask && etype == Inkscape::LivePathEffect::POWERMASK) {
        return false;
    }

    if (item_type == "group" && !converter.get_on_group(etype)) {
        return false;
    } else if (item_type == "shape" && !converter.get_on_shape(etype)) {
        return false;
    } else if (item_type == "path" && !converter.get_on_path(etype)) {
        return false;
    }

    return true;
}

} // namespace

namespace Linea {

std::vector<LPEMetadata> get_list_of_applicable_lpes(SPLPEItem* item, bool use, bool include_experimental) {
    auto shape = cast<SPShape>(item);
    auto path = cast<SPPath>(item);
    auto group = cast<SPGroup>(item);
    bool has_clip = item && item->getClipObject() != nullptr;
    bool has_mask = item && item->getMaskObject() != nullptr;

    Glib::ustring item_type;
    if (group) {
        item_type = "group";
    } else if (path) {
        item_type = "path";
    } else if (shape) {
        item_type = "shape";
    } else if (use) {
        item_type = "use";
    }

    const auto& converter = Inkscape::LivePathEffect::LPETypeConverter;
    auto lpes = std::vector<LPEMetadata>{};
    lpes.reserve(converter._length);
    for (int i = 0; i < static_cast<int>(converter._length); ++i) {
        const auto* const data = &converter.data(i);
        const auto& type = data->id;
        const auto& untranslated_label = converter.get_label(type);

        auto category = converter.get_category(type);
        if (sp_has_fav(untranslated_label)) {
            category = Inkscape::LivePathEffect::LPECategory::Favorites;
        }

        if (!include_experimental && category == Inkscape::LivePathEffect::LPECategory::Experimental) {
            continue;
        }

        Glib::ustring label = g_dpgettext2(0, "path effect", untranslated_label.c_str());
        const auto& icon = converter.get_icon(type);
        auto tooltip = get_tooltip(type, untranslated_label);
        const auto sensitive = can_apply(converter, type, item_type, has_clip, has_mask);
        lpes.push_back(LPEMetadata{type, category, std::move(label), icon, std::move(tooltip), sensitive});
    }

    return lpes;
}

} // namespace Linea
