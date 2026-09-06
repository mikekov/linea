// SPDX-License-Identifier: GPL-2.0-or-later
//
// SelectionStateModel: owns the SelectionState snapshot for one desktop.
// Replaces SPDesktop::fireStyleChanged.
//
//  - rebuilds the snapshot in ONE pass over the selection
//  - coalesces modification bursts (a drag emits one rebuild per event-loop
//    turn, not one per connectModified callback)
//  - emits (state, delta, origin_tags); delta-empty rebuilds emit nothing
//
// Intended to live as a member of SPDesktop, but has no desktop dependencies
// beyond Selection, so it can move later.

#ifndef LINEA_PROPS_SELECTION_STATE_MODEL_H
#define LINEA_PROPS_SELECTION_STATE_MODEL_H

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <sigc++/sigc++.h>

#include "props/edit-target.h"
#include "props/selection-state.h"

class QObject;
class SPDesktop;

namespace Inkscape {
class Selection;
}

namespace Linea::Props {

class SelectionStateModel {
public:
    explicit SelectionStateModel(SPDesktop* desktop);
    ~SelectionStateModel();

    const SelectionState& state() const { return _current; }

    // origin_tags: SP_OBJECT_USER_MODIFIED_TAG_n bits extracted from the
    // modification flags — identify which Editor(s) caused the change
    // (0 = external: canvas tool, undo/redo, script). Binders use them for
    // echo suppression.
    using ChangedSignal = sigc::signal<void (const SelectionState&, const SelectionDelta&, unsigned origin_tags)>;
    ChangedSignal& signalChanged() { return _signal_changed; }

    // Text tool integration: the scope lambdas are evaluated on each rebuild.
    // readScope returns the overlapping tspans for the snapshot (every character
    // in a span shares that span's style, so this is correct for reading).
    // targetScope returns the write-side EditTarget (a text-range target that
    // routes through sp_te_apply_style) when the text tool has a subselection.
    // SPDesktop sets both once with lambdas that dynamically check the current
    // tool; the model watches tool changes via connectEventContextChanged and
    // re-evaluates the scopes on each rebuild.
    using ItemScope = std::function<std::vector<SPItem*>()>;
    using TargetScope = std::function<std::optional<EditTarget>()>;
    void setTextScope(ItemScope scope);
    void setTargetScope(TargetScope scope);

    ItemScope textScope() const { return _textScope; }
    TargetScope targetScope() const { return _targetScope; }

private:
    // Collects flags and schedules rebuildNow() once per event-loop turn.
    void scheduleRebuild(unsigned flags);
    void rebuildNow();

    // Visits the items contributing to the snapshot: selected items, with
    // text objects replaced by the active spans when a text scope is set.
    // Returns true if a text scope was active and items were visited.
    bool forEachLeafItem(const std::function<void(SPObject*)>& fn);

    Inkscape::Selection* _selection = nullptr;
    SPDesktop* _desktop = nullptr;
    SelectionState _current;
    ItemScope _textScope;
    TargetScope _targetScope;
    unsigned _pendingFlags = 0;
    bool _rebuildScheduled = false;
    ChangedSignal _signal_changed;
    std::unique_ptr<QObject> _timerGuard;  // cancels the scheduled rebuild on destruction
    sigc::scoped_connection _changedConn;
    sigc::scoped_connection _modifiedConn;
    sigc::scoped_connection _toolChangedConn;
    sigc::scoped_connection _cursor_moved;
    tools_enum _activeTool = TOOLS_INVALID;
};

} // namespace Linea::Props

#endif // LINEA_PROPS_SELECTION_STATE_MODEL_H
