// SPDX-License-Identifier: GPL-2.0-or-later

#include "props/edit-target.h"

#include "text-editing.h"     // sp_te_apply_style
#include "util/paint-item-ops.h"  // set_item_style

namespace Linea::Props {

void EditTarget::applyCss(SPCSSAttr* css) const {
    if (!css) return;

    if (!_isTextRange) {
        Util::set_item_style(_item, css);
        return;
    }

    // sp_te_apply_style does its own transform compensation (scales the CSS
    // by 1/descrim internally) and calls te_update_layout_now_recursive
    // (rebuildLayout + updateRepr) before returning. Nothing extra needed.
    sp_te_apply_style(_item, _start, _end, css);
}

} // namespace Linea::Props
