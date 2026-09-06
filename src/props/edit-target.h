// SPDX-License-Identifier: GPL-2.0-or-later
//
// EditTarget: the write-side abstraction for one editable target.
//
// The Props pipeline writes properties through PropertyDef::apply, which is
// invoked once per target. Most targets are plain SPItems, but when the text
// tool has a cursor-range subselection the target is a (text-root, start, end)
// triple: styling must go through sp_te_apply_style, which splits text nodes
// to match the range. Those split nodes do not exist before the call, so no
// vector<SPItem*> scope can represent a true text subselection — EditTarget
// replaces that fiction with a single choke point, applyCss, that both paths
// share.

#ifndef LINEA_PROPS_EDIT_TARGET_H
#define LINEA_PROPS_EDIT_TARGET_H

#include "libnrtype/Layout-TNG.h"

class SPCSSAttr;
class SPItem;

namespace Linea::Props {

class EditTarget {
public:
    EditTarget() = delete;

    // Plain item target: CSS lands on the item via Util::set_item_style.
    explicit EditTarget(SPItem* item)
        : _item(item), _isTextRange(false) { assert(item); }

    // Text-range target: CSS is applied via sp_te_apply_style(text, start, end, css),
    // which splits tspans to match the range. \p text is the root <text>/<flowRoot>.
    EditTarget(SPItem* text, Inkscape::Text::Layout::iterator start,
               Inkscape::Text::Layout::iterator end)
        : _item(text), _isTextRange(true), _start(start), _end(end) { assert(text); }

    // The item: for a plain target, the item itself; for a text range, the
    // text root. Readers and non-CSS appliers use this (cast<> returns null
    // for shape types on a text root, so geometry appliers no-op correctly).
    SPItem* item() const { return _item; }

    bool isTextRange() const { return _isTextRange; }

    // Single choke point for CSS writes. For a plain item this is equivalent
    // to Util::set_item_style (transform-aware). For a text range it
    // scale-compensates against the text root's transform, calls
    // sp_te_apply_style, then rebuilds the layout and updates the repr.
    void applyCss(SPCSSAttr* css) const;

private:
    SPItem* _item = nullptr;
    bool _isTextRange = false;
    Inkscape::Text::Layout::iterator _start;
    Inkscape::Text::Layout::iterator _end;
};

} // namespace Linea::Props

#endif // LINEA_PROPS_EDIT_TARGET_H
