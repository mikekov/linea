// SPDX-License-Identifier: GPL-2.0-or-later

#include "props/edit-target.h"

#include "object/sp-item.h"
#include "object/sp-root.h"
#include "text-editing.h"     // sp_te_apply_style
#include "util/cast.h"
#include "util/paint-item-ops.h"  // set_item_style

namespace Linea::Props {

EditTarget::EditTarget(SPItem* item)
    : _item(item), _object(item), _isTextRange(false) { assert(item); }

EditTarget::EditTarget(SPItem* text, Inkscape::Text::Layout::iterator start,
                       Inkscape::Text::Layout::iterator end)
    : _item(text), _object(text), _isTextRange(true), _start(start), _end(end) {
    assert(text);
}

EditTarget::EditTarget(SPObject* object)
    : _item(is<SPRoot>(object) ? nullptr : cast<SPItem>(object))
    , _object(object)
    , _isTextRange(false) {
    assert(object);
}

void EditTarget::applyCss(SPCSSAttr* css) const {
    if (!css || !_item) return;

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
