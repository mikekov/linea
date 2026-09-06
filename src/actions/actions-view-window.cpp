// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Gio::Actions for window handling that are not useful from the command line (thus tied to window map).
 * Found under the "View" menu.
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-view-window.h"
#include "action-meta.h"
#include "action-registry.h"

#include <array>
#include <glibmm/i18n.h>

#include "linea-window.h"
#include "ui/desktop/desktop-widget.h"

namespace {

#if 0
void window_previous(LineaWindow *win)
{
    INKSCAPE.switch_desktops_prev();
}

void window_next(LineaWindow *win)
{
    INKSCAPE.switch_desktops_next();
}

void window_new(LineaWindow *win)
{
    auto app = InkscapeApplication::instance();
    auto doc = app->get_active_document();
    if (!doc) {
        return;
    }
    app->desktopOpen(doc);
}
#endif

void tab_previous(LineaWindow *win)
{
    win->getDesktopWidget()->advanceTab(-1);
}

void tab_next(LineaWindow *win)
{
    win->getDesktopWidget()->advanceTab(1);
}

#if 0
const Glib::ustring SECTION = NC_("Action Section", "View");

auto const raw_data_view_window = std::vector<std::vector<Glib::ustring>>
{
    // clang-format off
    {"win.window-new",      N_("Duplicate Window"), SECTION, N_("Open a new window with the same document")},
    {"win.window-previous", N_("Previous Window"),  SECTION, N_("Switch to the previous document window")},
    {"win.window-next",     N_("Next Window"),      SECTION, N_("Switch to the next document window")},
    {"win.tab-next",        N_("Next Tab"),         SECTION, N_("Switch to the next document tab")},
    {"win.tab-previous",    N_("Previous Tab"),     SECTION, N_("Switch to the previous document tab")},
    // clang-format on
};

} // namespace

void add_actions_view_window(LineaWindow* win)
{
    // clang-format off
    win->add_action("window-new",                  sigc::bind(sigc::ptr_fun(&window_new),       win));
    win->add_action("window-previous",             sigc::bind(sigc::ptr_fun(&window_previous),  win));
    win->add_action("window-next",                 sigc::bind(sigc::ptr_fun(&window_next),      win));
    win->add_action("tab-next",                    sigc::bind(sigc::ptr_fun(&tab_next),         win));
    win->add_action("tab-previous",                sigc::bind(sigc::ptr_fun(&tab_previous),     win));
    // clang-format on

    // Check if there is already an application instance (GUI or non-GUI).
    auto app = InkscapeApplication::instance();
    if (!app) {
        show_output("add_actions_view_window: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_view_window);
}
#endif

Glib::ustring const SECTION = NC_("Action Section", "View");

const auto tabActions =  std::to_array<WindowActionDef>({
    // clang-format off
    {"tab-next",     N_("Next Tab"),     SECTION, N_("Switch to the next document tab"),     tab_next},
    {"tab-previous", N_("Previous Tab"), SECTION, N_("Switch to the previous document tab"), tab_previous}
    // clang-format on
});

} // namespace

void add_actions_view_window(LineaWindow* wnd) {
    if (!wnd) return;

    auto& registry = ActionRegistry::get();

    for (const auto& m : tabActions) {
        auto action = registry.createAction(m, [cb = m.callback, wnd] { cb(wnd); });
        wnd->addAction(action);
    }

}
