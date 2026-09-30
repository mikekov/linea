// SPDX-License-Identifier: GPL-2.0-or-later
/** \file SnapshotContext
 * Machinery shared by the display trees (Drawing/DrawingItem and
 * CanvasItemContext/CanvasItem) for deferring structural changes while a
 * rendering snapshot is in progress.
 */
#ifndef LINEA_UTIL_SNAPSHOT_CONTEXT_H
#define LINEA_UTIL_SNAPSHOT_CONTEXT_H

#include <cassert>
#include <utility>
#include <vector>

#include "util/funclog.h"

namespace Inkscape {
namespace Util {

/**
 * Base class for the owner of a display tree that is frozen while the canvas
 * renders it on background threads.
 *
 * While snapshotted, mutations are logged in a FuncLog and replayed on
 * unsnapshot(). Items unlinked during the replay are detached in log order but
 * destroyed only after the whole log has run: entries logged after an item's
 * unlink() may still reference it, so it must not be freed early.
 */
template <typename Item>
class SnapshotContext {
public:
    void snapshot() {
        assert(!_snapshotted);
        _snapshotted = true;
    }

    void unsnapshot() {
        assert(_snapshotted);
        _snapshotted = false; // Unsnapshot before replaying log so further work is not deferred.
        _funclog_replaying = true;
        _funclog();
        _funclog_replaying = false;
        drain_obsolete();
    }

    bool snapshotted() const { return _snapshotted; }

    /// Run f now, or defer it until unsnapshot() if snapshotted.
    template <typename F>
    void defer(F&& f) {
        _snapshotted ? _funclog.emplace(std::forward<F>(f)) : f();
    }

    /// Delete an item, or defer destruction until the funclog replay finishes.
    void delete_item(Item* item) {
        if (!item) return;

        if (_funclog_replaying) {
            _obsolete_items.push_back(item);
        } else {
            delete item;
        }
    }

protected:
    /// Destroy items left over by an abandoned replay.
    void drain_obsolete() {
        while (!_obsolete_items.empty()) {
            delete _obsolete_items.back();
            _obsolete_items.pop_back();
        }
    }

private:
    bool _snapshotted = false;
    bool _funclog_replaying = false;
    std::vector<Item*> _obsolete_items;
    FuncLog _funclog;

    /*
     * Simple cacheline separator compatible with x86 (64 bytes) and M* (128 bytes).
     * Keeps the snapshot/funclog state off cachelines shared with the host's other
     * members, which render threads may be reading concurrently.
     */
    char cacheline_separator[127];
};

} // namespace Util
} // namespace Inkscape

#endif // LINEA_UTIL_SNAPSHOT_CONTEXT_H
