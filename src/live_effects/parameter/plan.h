// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Live Path Effect UI build plan.
 *
 * A declarative tree describing the layout of an LPE's parameter panel.
 * Each LPE constructor populates a plan; Effect::newWidget2() interprets it.
 *
 * Copyright (C) 2026 Michael Kowalski
 */

#ifndef LINEA_LIVEPATHEFFECT_PLAN_H
#define LINEA_LIVEPATHEFFECT_PLAN_H

#include <functional>
#include <variant>
#include <vector>

#include <glibmm/ustring.h>

namespace Inkscape::LivePathEffect {

class Parameter;

/// A leaf node referencing a parameter widget.
struct PlanParam {
    Parameter* param;
    int colspan = 1;  ///< Grid column span (0 = single column).
    std::function<void()> on_changed;  ///< Optional extra callback after value changes.
};

/// A leaf node for an action button.
struct PlanButton {
    Glib::ustring label;
    std::function<void()> callback;
    Glib::ustring icon;       ///< Optional icon name; empty = no icon.
    Glib::ustring tooltip;    ///< Optional tooltip; empty = none.
    bool checkable = false;
    bool is_checked = false;
    int colspan = 1;  ///< Grid column span (0 = single column).
};

/// A single toggle/radio button in a radio group.
struct PlanRadioItem {
    Glib::ustring label;      ///< Text label; may be empty if icon-only.
    Glib::ustring icon;       ///< Optional icon name.
    Glib::ustring tooltip;    ///< Optional tooltip.
    std::function<void()> on_select;  ///< Called when this item is selected.
    bool active = false;      ///< Initial checked state.
};

/// A spacer/gap in the layout — advances row or column depending on parent orientation.
struct PlanGap {
    int size = 0;  ///< Spacing in pixels (0 = just advance to next cell).
};

/// A group of mutually-exclusive radio toggle buttons, laid out horizontally.
struct PlanRadioGroup;

/// A group node — optionally labeled container holding child plan nodes.
struct PlanGroup;

/// A tabbed container — renders groups as tabs in a QTabWidget.
struct PlanNotebook;

/// A node in the UI build plan.
using PlanNode = std::variant<PlanParam, PlanButton, PlanRadioItem, PlanRadioGroup, PlanGroup, PlanNotebook, PlanGap>;

/// A group of mutually-exclusive radio toggle buttons, laid out horizontally.
struct PlanRadioGroup {
    Glib::ustring label;      ///< Empty = no visible label/frame title.
    std::vector<PlanNode> items;
    int colspan = 1;  ///< Grid column span (0 = single column).
    bool vertical = false;
};

/// A group node — optionally labeled container holding child plan nodes.
struct PlanGroup {
    Glib::ustring label;      ///< Empty = no visible label/frame title.
    std::vector<PlanNode> children;
    bool vertical = true;
};

/// A tabbed container — renders groups as tabs in a QTabWidget.
struct PlanNotebook {
    std::vector<PlanGroup> pages;
    int current_page = 0;      ///< Initial active page index.
    std::function<void(int)> on_page_changed;  ///< Optional callback when tab changes.
};

// --- Convenience builders ---

inline PlanNode p(Parameter* param, int colspan, std::function<void()> on_changed = {}) {
    return PlanParam{param, colspan, std::move(on_changed)};
}

inline PlanNode p(Parameter* param, std::function<void()> on_changed = {}) {
    return PlanParam{param, 1, std::move(on_changed)};
}

inline PlanNode btn(Glib::ustring label, std::function<void()> callback,
                    Glib::ustring icon = {}, Glib::ustring tooltip = {}, int colspan = 1) {
    return PlanButton{std::move(label), std::move(callback), std::move(icon), std::move(tooltip), false, false, colspan};
}

inline PlanNode chk_btn(Glib::ustring label, std::function<void()> callback,
                    Glib::ustring icon = {}, Glib::ustring tooltip = {}, bool is_checked = false, int colspan = 1) {
    return PlanButton{std::move(label), std::move(callback), std::move(icon), std::move(tooltip), true, is_checked, colspan};
}

inline PlanNode grp(Glib::ustring label, std::vector<PlanNode> children) {
    return PlanGroup{std::move(label), std::move(children), true};
}

inline PlanNode grp(std::vector<PlanNode> children) {
    return PlanGroup{{}, std::move(children), true};
}

inline PlanNode row(std::vector<PlanNode> children) {
    return PlanGroup{{}, std::move(children), false};
}

inline PlanNode radiogroup(std::vector<PlanNode> items, int colspan = 1) {
    // plain radio group with horizontal layout
    return PlanRadioGroup{{}, std::move(items), colspan, false};
}

inline PlanNode radiogroup(Glib::ustring label, std::vector<PlanNode> items, int colspan = 1) {
    // radio group with label and vertical layout
    return PlanRadioGroup{std::move(label), std::move(items), colspan, true};
}

inline PlanNode gap(int size = 10) {
    return PlanGap{size};
}

/// Like grp() but requires a label — for use as a notebook page.
inline PlanGroup page(Glib::ustring label, std::vector<PlanNode> children) {
    return PlanGroup{std::move(label), std::move(children), true};
}

inline PlanNode notebook(std::vector<PlanGroup> pages, int current_page = 0,
                         std::function<void(int)> on_page_changed = {}) {
    return PlanNotebook{std::move(pages), current_page, std::move(on_page_changed)};
}

} // namespace Inkscape::LivePathEffect

#endif // LINEA_LIVEPATHEFFECT_PLAN_H
