// SPDX-License-Identifier: GPL-2.0-or-later
//
// Editor: the single write pipeline. Every edit — simple property writes and
// structural paint operations — goes through here, so iteration, undo
// coalescing, modification tagging and echo suppression live in ONE place.
//
// One Editor per panel group, each with its own SP_OBJECT_USER_MODIFIED_TAG_n,
// preserving the current per-panel undo-coalescing behavior.

#ifndef LINEA_PROPS_EDITOR_H
#define LINEA_PROPS_EDITOR_H

#include <functional>
#include <optional>

#include "desktop.h"
#include "document-undo.h"
#include "selection.h"
#include "ui/operation-blocker.h"
#include "props/edit-target.h"
#include "props/property-def.h"
#include "util/paint-item-ops.h"

namespace Linea::Props {

class Editor {
public:
    Editor(SPDesktop* desktop, unsigned tag) : _desktop(desktop), _tag(tag) {}

    void setDesktop(SPDesktop* desktop) { _desktop = desktop; }

    // Value write: apply `def` to every target in scope, one undo step
    // (coalesced by def.undo_key), tagged so the resulting model update
    // carries origin_tag == _tag.
    template <typename T>
    void set(const PropertyDef<T>& def, const T& value) {
        if (!_desktop || _blocker.pending()) return;
        auto scoped = _blocker.block();

        forEachTarget([&](const EditTarget& target) { def.apply(target, value); });

        Inkscape::DocumentUndo::maybeDone(_desktop->getDocument(), def.undo_key,
                                          def.undo_label(), "", _tag);
    }

    // Live-preview variant: same write, no undo step. Call commit() when the
    // interaction ends (slider release, gradient drag end).
    template <typename T>
    void preview(const PropertyDef<T>& def, const T& value) {
        if (!_desktop || _blocker.pending()) return;
        auto scoped = _blocker.block();
        forEachTarget([&](const EditTarget& target) { def.apply(target, value); });
        _pendingUndo = true;
    }
    void commit(const char* undo_key, Inkscape::Util::Internal::ContextString label);

    // Structural paint edits (gradients, patterns, hatches, swatches, ...).
    // Self-contained: applies the op to every target and records undo in one
    // call. The caller passes the context-appropriate undo key and label.
    SPPaintServer* apply(const Util::PaintEditDelegate::Op& op,
                         const char* undo_key, Inkscape::Util::Internal::ContextString undo_label);

    // Scope override: by default edits target getSelection()->items().
    // Tools redirect edits (e.g. text tool -> active text range) by installing
    // a scope that returns EditTargets — this is what PaintEditDelegate
    // subclasses actually varied.
    using TargetScope = std::function<std::optional<EditTarget>()>;
    void setTargetScope(TargetScope scope) { _scope = std::move(scope); }

    unsigned tag() const { return _tag; }
    bool busy() const { return _blocker.pending(); }
    SPDesktop* desktop() const { return _desktop; }

private:
    template <typename F>
    void forEachTarget(F&& fn) {
        if (_scope) {
            if (auto target = _scope()) {
                fn(*target);
                return;
            }
        }
        for (auto item : _desktop->getSelection()->items()) {
            if (item) {
                fn(EditTarget{item});
            }
        }
    }

    SPDesktop* _desktop = nullptr;
    TargetScope _scope;
    OperationBlocker _blocker;
    unsigned _tag = 0;
    bool _pendingUndo = false;
};

} // namespace Linea::Props

#endif // LINEA_PROPS_EDITOR_H
