// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LIVE_EFFECTS_LPE_COMMON_H
#define LIVE_EFFECTS_LPE_COMMON_H

#include <vector>
#include <glibmm/ustring.h>

#include "live_effects/effect-enum.h"

class SPLPEItem;

namespace Linea {

struct LPEMetadata {
    Inkscape::LivePathEffect::EffectType type{};
    Inkscape::LivePathEffect::LPECategory category{};
    Glib::ustring label, icon_name, tooltip;
    bool sensitive{};
};

std::vector<LPEMetadata> get_list_of_applicable_lpes(SPLPEItem* item, bool use, bool include_experimental);

} // namespace Linea

#endif // LIVE_EFFECTS_LPE_COMMON_H
