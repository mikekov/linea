// SPDX-License-Identifier: GPL-2.0-or-later
//
// PropertyDef: the compile-time descriptor of one editable property —
// where to read it in the SelectionState snapshot, how to apply it to a
// single item, and how to record the edit for undo.
//
// One `inline constexpr` instance per editable table row is generated below;
// panels reference them as Props::rect_rx etc. A wrong field/type/applier
// combination is a compile error.
//
// Note for translators/tooling: undo labels live in the *.def files as
// RC_("Undo", ...) literals; add the .def files to POTFILES so xgettext
// picks them up.

#ifndef LINEA_PROPS_PROPERTY_DEF_H
#define LINEA_PROPS_PROPERTY_DEF_H

#include "props/edit-target.h"
#include "props/selection-state.h"
#include "util-string/context-string.h"

class SPItem;

namespace Linea::Props {

template <typename T>
struct PropertyDef {
    using value_type = T;

    Field field;                                             // delta bit for this property
    const mixed_property<T>& (*get)(const SelectionState&);  // read from snapshot
    void (*apply)(const EditTarget&, const T&);              // write to ONE target
    const char* undo_key;                                    // maybeDone coalescing key
    Inkscape::Util::Internal::ContextString (*undo_label)(); // deferred: translated at edit time
};

// --- Generated property definitions ------------------------------------------
// Editable rows produce a PropertyDef; LINEA_PROP_RO rows produce nothing here
// (their fields are still queried, diffed and bindable read-only via Field).

#define LINEA_PROP(type, name, reader, applier, key, label)                       \
    inline constexpr PropertyDef<type> name{                                      \
        Field::name,                                                              \
        [](const SelectionState& s) -> const mixed_property<type>& {              \
            return s.element.name;                                                \
        },                                                                        \
        LINEA_STRIP(applier),                                                     \
        key,                                                                      \
        [] { return label; }};
#define LINEA_PROP_RO(type, name, reader)
#include "props/element-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

#define LINEA_PROP(type, name, reader, applier, key, label)                       \
    inline constexpr PropertyDef<type> name{                                      \
        Field::name,                                                              \
        [](const SelectionState& s) -> const mixed_property<type>& {              \
            return s.style.name;                                                  \
        },                                                                        \
        LINEA_STRIP(applier),                                                     \
        key,                                                                      \
        [] { return label; }};
#define LINEA_PROP_RO(type, name, reader)
#include "props/presentation-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

#define LINEA_PROP(type, name, reader, applier, key, label)                       \
    inline constexpr PropertyDef<type> name{                                      \
        Field::name,                                                              \
        [](const SelectionState& s) -> const mixed_property<type>& {              \
            return s.typography.name;                                             \
        },                                                                        \
        LINEA_STRIP(applier),                                                     \
        key,                                                                      \
        [] { return label; }};
#define LINEA_PROP_RO(type, name, reader)
#include "props/typography-props.def"
#undef LINEA_PROP
#undef LINEA_PROP_RO

} // namespace Linea::Props

#endif // LINEA_PROPS_PROPERTY_DEF_H
