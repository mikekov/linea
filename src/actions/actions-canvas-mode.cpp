// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for changing the canvas display mode. Tied to a particular LineaWindow.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "actions-canvas-mode.h"

#include <array>
#include <iostream>

#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include <glibmm/i18n.h>

#include "action-meta.h"
#include "action-registry.h"
#include "ui/interface.h"

#include "actions-helper.h"

#include "desktop.h"
// #include "inkscape-application.h"
#include "linea-window.h"

#include "display/rendermode.h"
#include "display/drawing.h"  // Setting gray scale parameters.
#include "display/control/canvas-item-drawing.h"

#include "ui/widget/canvas.h"

// TODO: Use action state rather than set variable in Canvas (via Desktop).
// TODO: Move functions from Desktop to Canvas.
// TODO: Canvas actions should belong to canvas (not window)!

/**
 * Helper function to set display mode.
 */
void canvas_set_display_mode(Inkscape::RenderMode value, LineaWindow* win) {
    if (!win->get_desktop()) return;
    // g_assert(value != Inkscape::RenderMode::size);
    // saction->change_state((int)value);
    win->get_desktop()->getCanvas()->set_render_mode(value);
    // win->get_desktop()->setRenderMode(Inkscape::RenderMode(value));
}

/**
 * Set display mode.
 */
// void
// canvas_display_mode(int value, LineaWindow *win)
// {
//     if (value < 0 || value >= (int)Inkscape::RenderMode::size) {
//         show_output(Glib::ustring("canvas_display_mode: value out of bound! : ") + Glib::ustring::format(value));
//         return;
//     }

//     auto action = win->lookup_action("canvas-display-mode");
//     if (!action) {
//         show_output("canvas_display_mode: action 'canvas-display-mode' missing!");
//         return;
//     }

//     auto saction = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
//     if (!saction) {
//         show_output("canvas_display_mode: action 'canvas-display-mode' not SimpleAction!");
//         return;
//     }

//     canvas_set_display_mode(Inkscape::RenderMode(value), win, saction);
// }

/**
 * Cycle between values.
 */
void canvas_display_mode_cycle(LineaWindow* win) {
    if (!win->get_desktop()) return;
    auto canvas = win->get_desktop()->getCanvas();
    // TODO: match order of UI instead
    auto current_mode = static_cast<int>(canvas->get_render_mode()) + 1;
    current_mode %= static_cast<int>(Inkscape::RenderMode::size);
    canvas->set_render_mode(static_cast<Inkscape::RenderMode>(current_mode));

    #if 0
    auto action = win->lookup_action("canvas-display-mode");
    if (!action) {
        show_output("canvas_display_mode_cycle: action 'canvas-display-mode' missing!");
        return;
    }

    auto saction = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
    if (!saction) {
        show_output("canvas_display_mode_cycle: action 'canvas-display-mode' not SimpleAction!");
        return;
    }

    int value = -1;
    saction->get_state(value);
    // TODO: match order of UI instead
    value++;
    value %= (int)Inkscape::RenderMode::size;

    saction->activate_variant(Glib::Variant<int>::create(value));
    #endif
}


/**
 * Toggle between normal and last set other value.
 */
void canvas_display_mode_toggle(LineaWindow* win) {
    if (!win->get_desktop()) return;
    auto canvas = win->get_desktop()->getCanvas();
    static Inkscape::RenderMode old_value = Inkscape::RenderMode::OUTLINE;
    auto mode = canvas->get_render_mode();
    if (mode == Inkscape::RenderMode::NORMAL) {
        canvas->set_render_mode(old_value);
    } else {
        old_value = mode;
        canvas->set_render_mode(Inkscape::RenderMode::NORMAL);
    }

    #if 0
    auto action = win->lookup_action("canvas-display-mode");
    if (!action) {
        show_output("canvas_display_mode_toggle: action 'canvas-display-mode' missing!");
        return;
    }

    auto saction = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
    if (!saction) {
        show_output("canvas_display_mode_toogle: action 'canvas-display-mode' not SimpleAction!");
        return;
    }

    static Inkscape::RenderMode old_value = Inkscape::RenderMode::OUTLINE;

    int value = -1;
    saction->get_state(value);
    int new_value = 0;
    const int normal = static_cast<int>(Inkscape::RenderMode::NORMAL);

    if (value == normal) {
        new_value = static_cast<int>(old_value);
    } else {
        old_value = Inkscape::RenderMode(value);
        new_value = normal;
    }
    saction->activate_variant(Glib::Variant<int>::create(new_value));
    #endif
}

/**
 * Set split mode.
 */
#if 0
void
canvas_split_mode(int value, LineaWindow *win)
{
    if (value < 0 || value >= (int)Inkscape::SplitMode::size) {
        show_output("canvas_split_mode: value out of bound! : " + Glib::ustring::format(value));
        return;
    }

    auto action = win->lookup_action("canvas-split-mode");
    if (!action) {
        show_output("canvas_split_mode: action 'canvas-split-mode' missing!");
        return;
    }

    auto saction = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
    if (!saction) {
        show_output("canvas_split_mode: action 'canvas-split-mode' not SimpleAction!");
        return;
    }

    // If split mode is already set to the requested mode, turn it off.
    int old_value = -1;
    saction->get_state(old_value);
    if (value == old_value) {
        value = (int)Inkscape::SplitMode::NORMAL;
    }

    saction->change_state(value);

    SPDesktop* dt = win->get_desktop();
    auto canvas = dt->getCanvas();
    canvas->set_split_mode(Inkscape::SplitMode(value));
}
#endif

void canvas_split_mode(LineaWindow* wnd, Inkscape::SplitMode mode) {
    auto canvas = wnd->get_desktop()->getCanvas();
    canvas->set_split_mode(mode);
}

/**
 * Set gray scale for canvas.
 */
void
canvas_color_mode_gray(LineaWindow *win)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gdouble r = prefs->getDoubleLimited("/options/rendering/grayscale/red-factor",   0.21,  0.0, 1.0);
    gdouble g = prefs->getDoubleLimited("/options/rendering/grayscale/green-factor", 0.72,  0.0, 1.0);
    gdouble b = prefs->getDoubleLimited("/options/rendering/grayscale/blue-factor",  0.072, 0.0, 1.0);
    gdouble grayscale_value_matrix[20] =
        { r, g, b, 0, 0,
          r, g, b, 0, 0,
          r, g, b, 0, 0,
          0, 0, 0, 1, 0 };
    SPDesktop* dt = win->get_desktop();
    dt->getCanvasDrawing()->get_drawing()->setGrayscaleMatrix(grayscale_value_matrix);
}

/**
 * Toggle Gray scale on/off.
 */
void canvas_color_mode_toggle(LineaWindow* wnd) {
    auto canvas = wnd->get_desktop()->getCanvas();
    auto mode = canvas->get_color_mode();
    canvas->set_color_mode(mode == Inkscape::ColorMode::GRAYSCALE ? Inkscape::ColorMode::NORMAL : Inkscape::ColorMode::GRAYSCALE);

#if 0
    auto action = win->lookup_action("canvas-color-mode");
    if (!action) {
        show_output("canvas_color_mode_toggle: action missing!");
        return;
    }

    auto saction = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
    if (!saction) {
        show_output("canvas_color_mode_toggle: action not SimpleAction!");
        return;
    }

    bool state = false;
    saction->get_state(state);
    state = !state;
    saction->change_state(state);

    if (state) {
        // Set gray scale parameters.
        canvas_color_mode_gray(win);
    }

    win->get_desktop()->setColorMode(state ? Inkscape::ColorMode::GRAYSCALE : Inkscape::ColorMode::NORMAL);
#endif
}

/**
 * Toggle pixel preview (drawing rendered at a capped resolution) on/off.
 */
void canvas_pixel_preview_toggle(LineaWindow* wnd) {
    auto drawing = wnd->get_desktop()->getCanvasDrawing();
    drawing->set_pixel_preview(!drawing->get_pixel_preview());
}

void canvas_pixel_preview_off(LineaWindow* wnd) {
    auto drawing = wnd->get_desktop()->getCanvasDrawing();
    drawing->set_pixel_preview(false);
}

/**
 * Turn on pixel preview (1.0 = 100%, 2.0 = 200%).
 */
void canvas_pixel_preview_cap(LineaWindow* wnd, double cap) {
    auto drawing = wnd->get_desktop()->getCanvasDrawing();
    drawing->set_pixel_preview(true);
    drawing->set_pixel_preview_cap(cap);
}

bool get_canvas_pixel_preview(LineaWindow* wnd) {
    return wnd->get_desktop()->getCanvasDrawing()->get_pixel_preview();
}

bool get_canvas_pixel_preview_off(LineaWindow* wnd) {
    return !wnd->get_desktop()->getCanvasDrawing()->get_pixel_preview();
}

bool get_canvas_pixel_preview_100(LineaWindow* wnd) {
    auto drawing = wnd->get_desktop()->getCanvasDrawing();
    return drawing->get_pixel_preview() && drawing->get_pixel_preview_cap() == 1.0;
}

bool get_canvas_pixel_preview_200(LineaWindow* wnd) {
    auto drawing = wnd->get_desktop()->getCanvasDrawing();
    return drawing->get_pixel_preview() && drawing->get_pixel_preview_cap() == 2.0;
}

/**
 * Toggle Color management on/off.
 */
void
canvas_color_manage_toggle(LineaWindow *win)
{
    auto action = win->lookup_action("canvas-color-manage");
    if (!action) {
        show_output("canvas_color_manage_toggle: action missing!");
        return;
    }

    auto saction = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
    if (!saction) {
        show_output("canvas_color_manage_toggle: action not SimpleAction!");
        return;
    }

    bool state = false;
    saction->get_state(state);
    state = !state;
    saction->change_state(state);

    // Save value as a preference
    Inkscape::Preferences *pref = Inkscape::Preferences::get();
    pref->setBool("/options/displayprofile/enable", state);

    SPDesktop* dt = win->get_desktop();
    auto canvas = dt->getCanvas();
    canvas->set_cms_active(state);
    canvas->redraw_all();
}

const Glib::ustring SECTION = NC_("Action Section", "Canvas Display");

struct CanvasModeEntry : public ActionMeta2 {
    void (*fn)(LineaWindow*);
    bool (*state)(LineaWindow*) = nullptr;
    const char* icon_name = nullptr;
};

static auto canvas_mode_entries = std::to_array<CanvasModeEntry>({
    // clang-format off
    {"canvas-display-mode-normal",          N_("Display Mode: Normal"),             SECTION,    N_("Use normal rendering mode"),
        [](LineaWindow* win) { canvas_set_display_mode(Inkscape::RenderMode::NORMAL, win); }},
    {"canvas-display-mode-outline",         N_("Display Mode: Outline"),            SECTION,    N_("Show only object outlines"),
        [](LineaWindow* win) { canvas_set_display_mode(Inkscape::RenderMode::OUTLINE, win); }},
    {"canvas-display-mode-no-filters",      N_("Display Mode: No Filters"),         SECTION,    N_("Do not render filters (for speed)"),
        [](LineaWindow* win) { canvas_set_display_mode(Inkscape::RenderMode::NO_FILTERS, win); }},
    {"canvas-display-mode-enhanced-lines",  N_("Display Mode: Enhance Thin Lines"), SECTION,    N_("Ensure all strokes are displayed on screen as at least 1 pixel wide"),
        [](LineaWindow* win) { canvas_set_display_mode(Inkscape::RenderMode::VISIBLE_HAIRLINES, win); }},
    {"canvas-display-mode-outline-overlay", N_("Display Mode: Outline Overlay"),    SECTION,    N_("Show objects as outlines, and the actual drawing below them with reduced opacity"),
        [](LineaWindow* win) { canvas_set_display_mode(Inkscape::RenderMode::OUTLINE_OVERLAY, win); }},

    {"canvas-display-mode-cycle",           N_("Display Mode: Cycle"),              SECTION,    N_("Cycle through display modes")                   ,
        [](auto win) { canvas_display_mode_cycle(win); }},

    {"canvas-display-mode-toggle",          N_("Toggle Outline Mode"),              SECTION,    N_("Toggle between normal and last non-normal mode"),
        [](auto win) { canvas_display_mode_toggle(win); }},

        //TODO:
    {"canvas-display-mode-toggle-preview",  N_("Display Mode: Toggle Preview"),     SECTION,    N_("Toggle between preview and previous mode")      },

    {"canvas-split-mode-off",               N_("Split Mode: Normal"),               SECTION,    N_("Do not split canvas"),
        [](auto win) { canvas_split_mode(win, Inkscape::SplitMode::NORMAL); }},
    {"canvas-split-mode-on",                N_("Split Mode: Split"),                SECTION,    N_("Render part of the canvas in outline mode"),
        [](auto win) { canvas_split_mode(win, Inkscape::SplitMode::SPLIT); }},
    {"canvas-split-mode-xray",             N_("Split Mode: X-Ray"),                SECTION,    N_("Render a circular area in outline mode"),
        [](auto win) { canvas_split_mode(win, Inkscape::SplitMode::XRAY); }},

    {"canvas-color-mode",                  N_("Color Mode"),                       SECTION,    N_("Toggle between normal and grayscale modes"),
        [](auto win) { canvas_color_mode_toggle(win); }},

    {"canvas-pixel-preview-toggle",        N_("Disabled"),             SECTION,    N_("Toggle pixel preview rendering"),
        [](auto win) { canvas_pixel_preview_toggle(win); }, get_canvas_pixel_preview_off},
    {"canvas-pixel-preview-off",           N_("Disabled"),                         SECTION,    N_("Disable pixel preview rendering"),
        [](auto win) { canvas_pixel_preview_off(win); }, get_canvas_pixel_preview_off},
    {"canvas-pixel-preview-100",           N_("Pixel ×1"),                         SECTION,    N_("Enable pixel preview (×1)"),
        [](auto win) { canvas_pixel_preview_cap(win, 1.0); }, get_canvas_pixel_preview_100},
    {"canvas-pixel-preview-200",           N_("Pixel ×2"),                         SECTION,    N_("Enable pixel preview (×2)"),
        [](auto win) { canvas_pixel_preview_cap(win, 2.0); }, get_canvas_pixel_preview_200},

    //TODO
    {"canvas-color-manage",                N_("Color Managed Mode"),               SECTION,    N_("Toggle between normal and color managed modes")    }
    // clang-format on
});

void add_actions_canvas_mode(LineaWindow* win) {
    auto& registry = ActionRegistry::get();

    for (auto& e : canvas_mode_entries) {
        QAction* a;
        if (e.state) {
            a = registry.createBoolAction(
                {e.id, e.label, e.tooltip, nullptr},
                [fn = e.fn, win](bool) { fn(win); },
                [state = e.state, win]() { return state(win); });
        } else {
            a = registry.createAction(e, [fn = e.fn, win]() { fn(win); });
        }
        win->addAction(a);
    }

#if 0
    // clang-format off
    win->add_action_radio_integer ("canvas-display-mode",                 sigc::bind(sigc::ptr_fun(&canvas_display_mode),                win), (int)Inkscape::RenderMode::NORMAL);
    win->add_action(               "canvas-display-mode-cycle",           sigc::bind(sigc::ptr_fun(&canvas_display_mode_cycle),          win));
    win->add_action(               "canvas-display-mode-toggle",          sigc::bind(sigc::ptr_fun(&canvas_display_mode_toggle),         win));
    win->add_action_radio_integer ("canvas-split-mode",                   sigc::bind(sigc::ptr_fun(&canvas_split_mode),                  win), (int)Inkscape::SplitMode::NORMAL);
    win->add_action_bool(          "canvas-color-mode",                   sigc::bind(sigc::ptr_fun(&canvas_color_mode_toggle),           win));
    win->add_action_bool(          "canvas-color-manage",                 sigc::bind(sigc::ptr_fun(&canvas_color_manage_toggle),         win), false);
    // clang-format on

    auto app = InkscapeApplication::instance();
    if (!app) {
        show_output("add_actions_canvas_mode: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_canvas_mode);
#endif
}
