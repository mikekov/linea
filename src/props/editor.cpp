// SPDX-License-Identifier: GPL-2.0-or-later

#include "props/editor.h"

#include <variant>

namespace Linea::Props {

void Editor::commit(const char* undo_key, Inkscape::Util::Internal::ContextString label) {
    if (!_desktop || !_pendingUndo) return;

    _pendingUndo = false;
    Inkscape::DocumentUndo::maybeDone(_desktop->getDocument(), undo_key, label, "", _tag);
}

SPPaintServer* Editor::apply(const Util::PaintEditDelegate::Op& op,
                             const char* undo_key, Inkscape::Util::Internal::ContextString undo_label) {
    if (!_desktop || _blocker.pending()) return nullptr;

    auto scoped = _blocker.block();

    // CssOp routes through target.applyCss so that text-range targets split
    // tspans via sp_te_apply_style (applying color to just the selected
    // characters). Structural ops (gradients, patterns, swatches, …) can't
    // be expressed per character range in SVG, so they use target.item()
    // (the text root for a text-range target), matching classic behavior.
    SPPaintServer* server = nullptr;
    if (std::holds_alternative<Util::PaintEditDelegate::CssOp>(op)) {
        const auto& css_op = std::get<Util::PaintEditDelegate::CssOp>(op);
        forEachTarget([&](const EditTarget& target) {
            target.applyCss(css_op.css.get());
        });
    } else {
        forEachTarget([&](const EditTarget& target) {
            auto s = Util::apply_paint_op_to_item(target.item(), op, _desktop, _tag);
            if (!server) server = s;
        });
    }

    Inkscape::DocumentUndo::maybeDone(_desktop->getDocument(), undo_key, undo_label, "", _tag);
    return server;
}

} // namespace Linea::Props
