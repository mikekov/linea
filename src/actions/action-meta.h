#ifndef LINEA_ACTION_META_H
#define LINEA_ACTION_META_H

#include <QString>
#include <glibmm/ustring.h>
#include <span>
#include <variant>

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
struct ActionDef;

struct ActionGroup {
    const char* id;           // "tools", "edit", "view"
    const char* section;      // N_("Tools"), N_("Edit")
    ActionScope scope;
    std::span<const ActionDef> actions;
};

// Callback type aliases
using AppCallback = void (*)(LineaApplication*);
using WindowCallback = void (*)(LineaWindow*);
using DocCallback = void (*)(SPDocument*);
using SelCallback = void (*)(Inkscape::Selection*);
using BoolCallback = void (*)(bool, InkscapeApplication*);
using StringParamCallback = void (*)(const QString&, InkscapeApplication*);
using IntParamCallback = void (*)(int, InkscapeApplication*);
using DoubleParamCallback = void (*)(double, InkscapeApplication*);

struct ActionDef {
    ActionMeta meta;
    std::variant<AppCallback, WindowCallback, DocCallback, SelCallback,
                 BoolCallback, StringParamCallback, IntParamCallback,
                 DoubleParamCallback> callback;
};

struct WindowActionDef : ActionMeta2 {
    WindowCallback callback;
    const char* icon_name = nullptr;
};

struct ApplicationActionDef : ActionMeta2 {
    AppCallback callback;
    const char* icon_name = nullptr;
};

// Parameterized action metadata (e.g., tool-switch with specific tool name)
struct ActionParamMeta : BoolActionMeta {
    const char* param;  // "Select", "Node", etc.
};

#endif
