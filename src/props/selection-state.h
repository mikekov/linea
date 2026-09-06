// SPDX-License-Identifier: GPL-2.0-or-later
//
// SelectionState: one strongly-typed snapshot of everything the UI can show
// about the current selection, built in a single pass over the selected items.
//
// The struct fields, the Field enum, the delta diff and the per-item merge
// are all generated from the property tables (element-props.def,
// presentation-props.def), so the read side can never drift from the tables.

#ifndef LINEA_PROPS_SELECTION_STATE_H
#define LINEA_PROPS_SELECTION_STATE_H

#include <bitset>
#include <cstdint>

#include <2geom/rect.h>

#include "util/mixed-property.h"
#include "props/accessors.h" // IWYU pragma: keep (readers used via LINEA_PROP macro expansion)
#include "ui/tools/tool-data.h"

class SPItem;

namespace Linea::Props {

// Strip the protective parentheses from a table column: LINEA_STRIP((x)) -> x
#define LINEA_STRIP(x) LINEA_STRIP_IMPL x
#define LINEA_STRIP_IMPL(...) __VA_ARGS__

// --- Field enum & delta -----------------------------------------------------

enum class Field : uint16_t {
#define LINEA_PROP(type, name, ...) name,
#define LINEA_PROP_RO(type, name, ...) name,
#include "props/element-props.def"
#include "props/presentation-props.def"
#include "props/typography-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO
    counts,   // selection composition changed (drives panel visibility)
    geometry, // selection bounding box changed (drives size widget refresh)
    _count
};

// One dirty bit per field; Binder pushes only dirty fields to widgets.
using SelectionDelta = std::bitset<static_cast<size_t>(Field::_count)>;

// Equality for delta purposes: two Mixed properties are "equal" even if their
// representative first-values differ — the widget shows mixed either way.
template <typename T>
bool equal(const mixed_property<T>& a, const mixed_property<T>& b) {
    if (a.is_mixed() || b.is_mixed()) return a.is_mixed() && b.is_mixed();
    if (a.is_unset() || b.is_unset()) return a.is_unset() && b.is_unset();
    return a.value() == b.value();
}

// --- Selection composition ----------------------------------------------------

struct Counts {
    int items = 0;
    int rectangles = 0;
    int ellipses = 0;
    int stars = 0;
    int polygons = 0;
    int paths = 0;
    int lines = 0;
    int groups = 0;
    int layers = 0;
    int svgs = 0;
    int pages = 0;
    int images = 0;
    int textual = 0;
    int flowtext = 0;
    tools_enum activeTool = TOOLS_INVALID;
    bool text_tool_active = false;       // text tool is the current tool
    bool has_text_subselection = false;  // text tool has an active span subselection
    bool operator == (const Counts&) const = default;
};

// Implemented in accessors.cpp (mirrors element-properties.cpp counting).
void merge_counts(Counts& counts, SPObject* item);

// --- State structs (generated fields) ---------------------------------------

struct ElementState {
#define LINEA_PROP(type, name, ...) mixed_property<type> name;
#define LINEA_PROP_RO(type, name, ...) mixed_property<type> name;
#include "props/element-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO
    Counts count;
};

struct PresentationState {
#define LINEA_PROP(type, name, ...) mixed_property<type> name;
#define LINEA_PROP_RO(type, name, ...) mixed_property<type> name;
#include "props/presentation-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO
};

// TypographyState is filled only when count.textual > 0, from the item scope
// the text tool dictates (whole objects or the active spans).
struct TypographyState {
#define LINEA_PROP(type, name, ...) mixed_property<type> name;
#define LINEA_PROP_RO(type, name, ...) mixed_property<type> name;
#include "props/typography-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO
};

struct SelectionState {
    bool empty() const { return element.count.items == 0; }
    bool pageSelection() const {
        const auto& count = element.count;
        // Note: currently it is not possible to select more than one page
        return count.items == 1 && count.pages + count.svgs == 1;
    }
    ElementState element;
    PresentationState style;
    TypographyState typography;
    Geom::OptRect bbox;  // selection bounding box in px (visual or geometric per preference)
    uint64_t revision = 0;
};

// --- Per-item merge (generated from the reader column) ----------------------

inline void merge_item(SelectionState& state, SPObject* object) {
    merge_counts(state.element.count, object);

    auto item = cast<SPItem>(object);
    if (!item) {
        // there's only one object-based property currently, it is handled here; SPPages are object-based
        if (auto id = read_id(object)) state.element.id.merge(std::move(*id));
        return;
    }

#define LINEA_PROP(type, name, reader, ...) \
    if (auto v = LINEA_STRIP(reader)(item)) state.element.name.merge(std::move(*v));
#define LINEA_PROP_RO(type, name, reader) \
    if (auto v = LINEA_STRIP(reader)(item)) state.element.name.merge(std::move(*v));
#include "props/element-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

#define LINEA_PROP(type, name, reader, ...) \
    if (auto v = LINEA_STRIP(reader)(item)) state.style.name.merge(std::move(*v));
#define LINEA_PROP_RO(type, name, reader) \
    if (auto v = LINEA_STRIP(reader)(item)) state.style.name.merge(std::move(*v));
#include "props/presentation-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

#define LINEA_PROP(type, name, reader, ...) \
    if (auto v = LINEA_STRIP(reader)(item)) state.typography.name.merge(std::move(*v));
#define LINEA_PROP_RO(type, name, reader) \
    if (auto v = LINEA_STRIP(reader)(item)) state.typography.name.merge(std::move(*v));
#include "props/typography-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO
}

// --- Delta computation (generated) -------------------------------------------

inline SelectionDelta diff(const SelectionState& a, const SelectionState& b) {
    SelectionDelta d;
#define LINEA_PROP(type, name, ...) \
    if (!equal(a.element.name, b.element.name)) d.set(static_cast<size_t>(Field::name));
#define LINEA_PROP_RO(type, name, ...) \
    if (!equal(a.element.name, b.element.name)) d.set(static_cast<size_t>(Field::name));
#include "props/element-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

#define LINEA_PROP(type, name, ...) \
    if (!equal(a.style.name, b.style.name)) d.set(static_cast<size_t>(Field::name));
#define LINEA_PROP_RO(type, name, ...) \
    if (!equal(a.style.name, b.style.name)) d.set(static_cast<size_t>(Field::name));
#include "props/presentation-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

#define LINEA_PROP(type, name, ...) \
    if (!equal(a.typography.name, b.typography.name)) d.set(static_cast<size_t>(Field::name));
#define LINEA_PROP_RO(type, name, ...) \
    if (!equal(a.typography.name, b.typography.name)) d.set(static_cast<size_t>(Field::name));
#include "props/typography-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

    if (!(a.element.count == b.element.count)) d.set(static_cast<size_t>(Field::counts));
    if (a.bbox != b.bbox) d.set(static_cast<size_t>(Field::geometry));
    return d;
}

} // namespace Linea::Props

#endif // LINEA_PROPS_SELECTION_STATE_H
