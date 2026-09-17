#ifndef LINEA_ACTION_META_H
#define LINEA_ACTION_META_H

#include <QString>
#include <glibmm/ustring.h>

// Forward declarations
class LineaWindow;
class SPDocument;
class InkscapeApplication;
class LineaApplication;

namespace Inkscape {
class Selection;
} // namespace Inkscape

enum class ActionScope { Application, Window, Document, Selection };

struct ActionMeta {
    // unique action ID
    const char* id;        // "tool-select"
    // UI label (translated)
    const char* label;     // N_("Select")
    // UI tooltip (translated)
    const char* tooltip;   // N_("Select objects")
    // Icon name, if any
    const char* icon_name;
};

struct BoolActionMeta : ActionMeta {
    // Label when checked (translated), nullptr for no swap
    const char* checked_label = nullptr;
};

struct ActionMeta2 {
    // unique action ID
    const char* id;        // "tool-select"
    // UI label (translated)
    const char* label;     // N_("Select")
    // section (translated)
    Glib::ustring section;
    // UI tooltip (translated)
    const char* tooltip;   // N_("Select objects")
};

// Group info for discovery (used by shortcuts UI)
struct ActionGroup {
    const char* id;           // "tools", "edit", "view"
    const char* section;      // N_("Tools"), N_("Edit")
    ActionScope scope;
};

template <typename Context>
struct ActionSpec {
    // unique action ID
    const char* id;
    // UI label (translated)
    const char* label;
    // section (translated)
    Glib::ustring section;
    // UI tooltip (translated)
    const char* tooltip;
    // icon name
    const char* icon_name;
    // action's callback
    void (*callback)(Context*);
    // optional state query
    bool (*state)(Context*) = nullptr;
    // optional checked state label
    const char* checked_label = nullptr;
};

// Parameterized action metadata (e.g., tool-switch with specific tool name)
struct ActionParamMeta : BoolActionMeta {
    const char* param;  // "Select", "Node", etc.
};

#endif
