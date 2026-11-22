// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RecentColors — a per-document most-recently-used color list.
 */

#ifndef LINEA_RECENT_COLORS_H
#define LINEA_RECENT_COLORS_H

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <sigc++/sigc++.h>

#include "colors/color.h"
#include "flat-color.h"

class SPDocument;

namespace Inkscape::XML {
class Node;
}

namespace Linea {

// The per-document list of recently applied colors: flat colors (stored
// opaque) and document swatches (stored by custom property name and
// resolved live against the stylesheet). Capacity-limited, most-recent
// first. Entries recorded under the same undo-commit serial replace the
// provisional head instead of pushing, so one undo step contributes at
// most one entry; the provisional head never evicts the older duplicate
// it is hiding. Changes to this list are never undoable and never mark
// the document modified.
class RecentColors {
public:
    static constexpr std::size_t capacity = 10;

    // Loads the persisted list from the document's namedview, if present.
    explicit RecentColors(SPDocument& document);

    // Record an applied color/swatch. undo_key is the key passed to
    // DocumentUndo::maybeDone for this change; nullptr marks a discrete
    // (non-coalescing) action which always pushes a new entry.
    void noteColor(const Inkscape::Colors::Color& color, const char* undo_key);
    void noteSwatch(const Swatch& swatch, const char* undo_key);

    // Follow a swatch rename so stored references keep resolving.
    void renameSwatch(const std::string& old_name, const std::string& new_name);

    // Entries most-recent first, swatches resolved against the current
    // stylesheet. Swatches no longer defined are skipped, as are older
    // duplicates of the head while they are kept.
    std::vector<FlatColor> entries() const;

    // Finalize the head, drop swatches that no longer resolve, and write
    // the list to (or remove it from) the given namedview repr.
    void save(Inkscape::XML::Node& namedview);

    // Emitted whenever the list changes (note/rename).
    sigc::signal<void()>& signal_changed() { return _signal_changed; }

private:
    // A flat color (opaque) or a swatch's custom property name (with "--").
    using Entry = std::variant<Inkscape::Colors::Color, std::string>;

    // Stable identity of an entry, doubling as its serialization: the
    // color's opaque string form, or the swatch's var() reference.
    static std::string _key(const Entry& entry);

    void _note(Entry entry, const char* undo_key);

    // Remove the head's older duplicates and trim to capacity; the head
    // is no longer provisional afterwards.
    void _finalizeHead();

    SPDocument& _document;
    std::deque<Entry> _entries;
    // The undo commit the provisional head belongs to: the document's
    // undoCommitSerial() and the maybeDone key at note time.
    std::optional<std::pair<unsigned long, std::string>> _head_commit;
    sigc::signal<void()> _signal_changed;
};

} // namespace Linea

#endif // LINEA_RECENT_COLORS_H
