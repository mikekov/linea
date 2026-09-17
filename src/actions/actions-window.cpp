// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for window handling tied to the application and with GUI.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-window.h"

#include <cstdlib>
#include <glibmm/i18n.h>

#include "actions/action-registry.h"
#include "linea-application.h"
#include "linea-window.h"

#if 0
#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include <glibmm/i18n.h>

#include "actions-helper.h"
#include "desktop.h"
#include "document.h"
#include "inkscape-application.h"
#include "linea-window.h"
#include "util-string/ustring-format.h"

// Actions for window handling (should be integrated with file dialog).

// Open a window for current document
void window_open(InkscapeApplication *app)
{
    auto document = app->get_active_document();
    if (document) {
        auto desktop = app->get_active_desktop();
        if (desktop && desktop->getDocument() && desktop->getDocument()->getVirgin()) {
            // We have a tab with an untouched template document, use this tab.
            app->document_swap(desktop, document);
        } else {
            app->desktopOpen(document);
        }
    } else {
        show_output("window_open(): failed to find document!");
    }
}

void window_query_geometry(InkscapeApplication *app)
{
    if (auto window = app->get_active_window()) {
        if (auto desktop = window->get_desktop()) {
            auto const [w, h] = desktop->getWindowSize();
            show_output(Glib::ustring("w:") + Inkscape::ustring::format_classic(w), false);
            show_output(Glib::ustring("h:") + Inkscape::ustring::format_classic(h), false);
        }
    } else {
        show_output("this action needs active window, probably you need to add --active-window / -q");
    }
}

void
window_set_geometry(const Glib::VariantBase& value, InkscapeApplication *app)
{
    Glib::Variant<Glib::ustring> s = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring> >(value);

    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(",", s.get());
    if (tokens.size() != 4) {
        show_output("action:set geometry: requires 'x, y, width, height'");
        return;
    }
    if (auto window = app->get_active_window()) {
        if (auto desktop = window->get_desktop()) {
            if (desktop->is_maximized()) {
                // QT TODO
                // desktop->getLineaWindow()->unmaximize();
            }
            int w = std::stoi(tokens[2]);
            int h = std::stoi(tokens[3]);
            desktop->setWindowSize({w, h});
        }
    } else {
        show_output("this action needs active window, probably you need to add --active-window / -q");
    }
}

void
window_close(InkscapeApplication *app)
{
    app->desktopCloseActive();
}

std::vector<std::vector<Glib::ustring>> hint_data_window =
{
    // clang-format off
    {"app.window-set-geometry",         N_("Enter comma-separated string for x, y, width, height")  }
    // clang-format on
};

const Glib::ustring SECTION = NC_("Action Section", "Window");

std::vector<std::vector<Glib::ustring>> raw_data_window =
{
    // clang-format off
    {"app.window-open",           N_("Window Open"),           SECTION, N_("Open a window for the active document; GUI only")       },
    {"app.window-close",          N_("Window Close"),          SECTION, N_("Close the active window, does not check for data loss") },
    {"app.window-query-geometry", N_("Window Query Geometry"), SECTION, N_("Query the active window's location and size") },
    {"app.window-set-geometry",   N_("Window Set Geometry"),   SECTION, N_("Set the active window's location and size (x, y, width, height)") },
    {"app.window-crash",          N_("Force Crash"),           SECTION, N_("Force Inkscape to crash, useful for testing.") },
    // clang-format on
};

void
add_actions_window(InkscapeApplication* app)
{
    auto *gapp = app->gio_app();
    Glib::VariantType String(Glib::VARIANT_TYPE_STRING);
    // clang-format off
    gapp->add_action(                "window-open",  sigc::bind(sigc::ptr_fun(&window_open),         app));
    gapp->add_action(                "window-close", sigc::bind(sigc::ptr_fun(&window_close),        app));
    gapp->add_action(                "window-query-geometry",  sigc::bind(sigc::ptr_fun(&window_query_geometry),       app));
    gapp->add_action_with_parameter( "window-set-geometry",    String, sigc::bind(sigc::ptr_fun(&window_set_geometry), app));
    gapp->add_action("window-crash", [=](){
        abort();
    });
    // clang-format on

    app->get_action_extra_data().add_data(raw_data_window);
    app->get_action_hint_data().add_data(hint_data_window);
}

#endif

namespace {

void window_crash(LineaApplication* /*app*/) {
    std::abort();
}

static auto window_action_defs = std::to_array<ActionSpec<LineaApplication>>({
    {"window-crash", N_("Force Crash"), NC_("Action Section", "Window"),
     N_("Force Linea to crash, useful for testing."), nullptr, window_crash},
});

} // namespace

void add_actions_window(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, window_action_defs);
}
