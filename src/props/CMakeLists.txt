# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(linea_base PRIVATE
    accessors.cpp
    binder.cpp
    edit-target.cpp
    editor.cpp
    selection-state-model.cpp

    # -------
    # Headers
    accessors.h
    binder.h
    conditions.h
    edit-target.h
    editor.h
    property-def.h
    selection-state.h
    selection-state-model.h
    widget-traits.h

    # Property tables
    element-props.def
    presentation-props.def
    typography-props.def
)
