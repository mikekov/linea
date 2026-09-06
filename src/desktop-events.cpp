// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Event handlers for SPDesktop.
 */
/* Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 1999-2010 Others
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "desktop-events.h"

#include <map>
#include <string>
#include <2geom/line.h>
#include <2geom/angle.h>
#include <glibmm/i18n.h>
#include <glibmm/ustring_hash.h>

#include <QGuiApplication>
#include <QInputDevice>

#include "desktop.h"
#include "document-undo.h"
#include "message-context.h"
#include "preferences.h"
#include "snap.h"
#include "actions/actions-tools.h"
#include "display/control/canvas-item-guideline.h"
#include "object/sp-guide.h"
#include "object/sp-namedview.h"
#include "ui/cursor-utils.h"
// #include "ui/dialog/guides.h"
#include "ui/tools/tool-base.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/select-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/events/canvas-event.h"
#include "ui/widget/events/debug.h"

using Inkscape::DocumentUndo;
using Inkscape::EventType;

static void snoop_extended(Inkscape::CanvasEvent const &event, SPDesktop *desktop);
static void init_extended();

bool sp_desktop_root_handler(Inkscape::CanvasEvent const &event, SPDesktop *desktop)
{
    if constexpr (Inkscape::DEBUG_EVENTS) {
        Inkscape::dump_event(event, "sp_desktop_root_handler");
    }

    static bool watch = false;
    static bool first = true;

    if (first) {
        auto prefs = Inkscape::Preferences::get();
        if (prefs->getBool("/options/useextinput/value", true) &&
            prefs->getBool("/options/switchonextinput/value"))
        {
            watch = true;
            init_extended();
        }
        first = false;
    }

    if (watch) {
        snoop_extended(event, desktop);
    }

    if (auto const tool = desktop->getTool()) {
        return tool->start_root_handler(event);
    }

    return false;
}

static Geom::Point drag_origin;
static SPGuideDragType drag_type = SP_DRAG_NONE;
static bool guide_moved = false;
static bool ruler_initiated = false; ///< True while a ruler-initiated guide drag is in progress.

bool sp_dt_guide_event(Inkscape::CanvasEvent const &event, Inkscape::CanvasItemGuideLine *guide_item, SPGuide *guide)
{
    if constexpr (Inkscape::DEBUG_EVENTS) {
        Inkscape::dump_event(event, "sp_dt_guide_event");
    }

    bool ret = false;

    auto desktop = guide_item->get_canvas()->get_desktop();
    if (!desktop) {
        std::cerr << "sp_dt_guide_event: No desktop!" << std::endl;
        return false;
    }

    // Limit to select/node tools only, unless this is a ruler-initiated drag.
    if (!ruler_initiated) {
        if (!dynamic_cast<Inkscape::UI::Tools::SelectTool *>(desktop->getTool()) &&
            !dynamic_cast<Inkscape::UI::Tools::NodeTool *>(desktop->getTool()))
        {
            return false;
        }
    }

    auto apply_snap = [desktop, guide] (Geom::Point &event_dt, unsigned modifiers) {
        // This is for snapping while dragging existing guidelines. New guidelines,
        // which are dragged off the ruler, are being snapped in sp_dt_ruler_event
        auto &m = desktop->getNamedView()->snap_manager;
        m.setup(desktop, true, guide, nullptr);
        if (drag_type == SP_DRAG_MOVE_ORIGIN) {
            // If we snap in guideConstrainedSnap() below, then motion_dt will
            // be forced to be on the guide. If we don't snap however, then
            // the origin should still be constrained to the guide. So let's do
            // that explicitly first:
            auto const line = Geom::Line(guide->getPoint(), guide->angle());
            auto const t = line.nearestTime(event_dt);
            event_dt = line.pointAt(t);
            if (!(modifiers & INK_SHIFT_MASK)) {
                m.guideConstrainedSnap(event_dt, *guide);
            }
        } else if (!(drag_type == SP_DRAG_ROTATE && modifiers & INK_CONTROL_MASK)) {
            // Cannot use shift here to disable snapping, because we already use it for rotating the guide.
            Geom::Point tmp;
            if (drag_type == SP_DRAG_ROTATE) {
                tmp = guide->getPoint();
                m.guideFreeSnap(event_dt, tmp, true, false);
                guide->moveto(tmp, false);
            } else {
                tmp = guide->getNormal();
                m.guideFreeSnap(event_dt, tmp, false, true);
                guide->set_normal(tmp, false);
            }
        }
        m.unSetup();
    };

    auto move_guide = [guide] (Geom::Point const &event_dt, unsigned modifiers, bool flag) {
        switch (drag_type) {
            case SP_DRAG_TRANSLATE:
            case SP_DRAG_MOVE_ORIGIN:
                guide->moveto(event_dt, flag);
                break;
            case SP_DRAG_ROTATE: {
                auto angle = Geom::Angle(event_dt - guide->getPoint());
                if (modifiers & INK_CONTROL_MASK) {
                    auto prefs = Inkscape::Preferences::get();
                    auto snaps = prefs->getDoubleLimited("/options/rotationsnapsperpi/value", 12.0, 0.1, 1800.0);
                    if (prefs->getBool("/options/relativeguiderotationsnap/value", false)) {
                        auto orig_angle = Geom::Angle(guide->getNormal());
                        auto snap_angle = angle - orig_angle;
                        double sections = std::floor(snap_angle.radians0() * snaps / M_PI + 0.5);
                        angle = M_PI / snaps * sections + orig_angle.radians0();
                    } else {
                        double sections = std::floor(angle.radians0() * snaps / M_PI + 0.5);
                        angle = M_PI / snaps * sections;
                    }
                }
                guide->set_normal(Geom::Point::polar(angle).cw(), flag);
                break;
            }
            default:
                assert(false);
                break;
        }
    };

    inspect_event(event,
        [&] (Inkscape::ButtonPressEvent const &event) {
            if (event.num_press == 2) {
                if (event.button == 1) {
                    drag_type = SP_DRAG_NONE;
                    desktop->getTool()->discard_delayed_snap_event();
                    guide_item->ungrab();
                    // QT TODO: Inkscape::UI::Dialog::GuidelinePropertiesDialog::showDialog(guide, desktop);
                    ret = true;
                }
            } else if (event.num_press == 1) {
                if (event.button == 1 && !guide->getLocked()) {
                    auto const event_dt = desktop->w2d(event.pos);

                    // Due to the tolerance allowed when grabbing a guide, event_dt will generally
                    // be close to the guide but not just exactly on it. The drag origin calculated
                    // here must be exactly on the guide line though, otherwise
                    // small errors will occur once we snap, see
                    // https://bugs.launchpad.net/inkscape/+bug/333762
                    drag_origin = Geom::projection(event_dt, Geom::Line(guide->getPoint(), guide->angle()));

                    if (event.modifiers & INK_SHIFT_MASK) {
                        // with shift we rotate the guide
                        drag_type = SP_DRAG_ROTATE;
                    } else if (event.modifiers & INK_CONTROL_MASK) {
                        drag_type = SP_DRAG_MOVE_ORIGIN;
                    } else {
                        drag_type = SP_DRAG_TRANSLATE;
                    }

                    if (drag_type == SP_DRAG_ROTATE || drag_type == SP_DRAG_TRANSLATE) {
                        guide_item->grab(EventType::BUTTON_RELEASE |
                                         EventType::BUTTON_PRESS   |
                                         EventType::MOTION);
                    }
                    ret = true;
                }
            }
        },

        [&] (Inkscape::MotionEvent const &event) {
            if (drag_type == SP_DRAG_NONE) {
                return;
            }

            desktop->getTool()->snap_delay_handler(guide_item, guide, event,
                                                           Inkscape::UI::Tools::DelayedSnapEvent::GUIDE_HANDLER);

            auto event_dt = desktop->w2d(event.pos);
            apply_snap(event_dt, event.modifiers);
            move_guide(event_dt, event.modifiers, false);

            guide_moved = true;
            desktop->set_coordinate_status(event_dt);
            desktop->getCanvas()->grab_focus();

            ret = true;
        },

        [&] (Inkscape::ButtonReleaseEvent const &event) {
            if (drag_type == SP_DRAG_NONE || event.button != 1) {
                return;
            }

            desktop->getTool()->discard_delayed_snap_event();

            if (guide_moved) {
                auto event_dt = desktop->w2d(event.pos);
                apply_snap(event_dt, event.modifiers);

                if (guide_item->get_canvas()->world_point_inside_canvas(event.pos)) {
                    move_guide(event_dt, event.modifiers, true);
                    DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Move guide"), "");
                } else {
                    // Undo movement of any attached shapes.
                    guide->moveto(guide->getPoint(), false);
                    guide->set_normal(guide->getNormal(), false);
                    guide->remove();
                    guide_item = nullptr;
                    desktop->getTool()->use_tool_cursor();

                    DocumentUndo::done(desktop->getDocument(), RC_("Undo", "Delete guide"), "");
                }

                guide_moved = false;
                desktop->set_coordinate_status(event_dt);
            }

            drag_type = SP_DRAG_NONE;
            ruler_initiated = false;
            if (guide_item) {
                guide_item->ungrab();
            }

            ret = true;
        },

        [&] (Inkscape::EnterEvent const &event) {
            auto const canvas = desktop->getCanvas();
            g_assert(canvas);

            // This is a UX thing. Check if the canvas has focus, so the user knows they can
            // use hotkeys. See issue: https://gitlab.com/inkscape/inkscape/-/issues/2439
            if (!guide->getLocked() /* && canvas->has_focus() */) {
                guide_item->set_stroke(guide->getHiColor());
            }

            // set move or rotate cursor
            if (guide->getLocked()) {
                Inkscape::set_svg_cursor(*canvas, "select.svg");
            } else if (event.modifiers & INK_SHIFT_MASK && drag_type != SP_DRAG_MOVE_ORIGIN) {
                Inkscape::set_svg_cursor(*canvas, "rotate.svg");
            } else {
                canvas->set_cursor(Qt::OpenHandCursor);
            }

            auto guide_description = guide->description();
            desktop->guidesMessageContext()->setF(Inkscape::NORMAL_MESSAGE, _("<b>Guideline</b>: %s"), guide_description);
            g_free(guide_description);
        },

        [&] (Inkscape::LeaveEvent const &event) {
            guide_item->set_stroke(guide->getColor());

            // restore event context's cursor
            desktop->getTool()->use_tool_cursor();

            desktop->guidesMessageContext()->clear();
        },

        [&] (Inkscape::KeyPressEvent const &event) {
            switch (Inkscape::UI::Tools::get_latin_keyval(event)) {
                case INK_KEY_Delete:
                case INK_KEY_KP_Delete:
                case INK_KEY_BackSpace:
                    if (!guide->getLocked()) {
                        auto doc = guide->document;
                        guide->remove();
                        guide_item = nullptr;
                        DocumentUndo::done(doc, RC_("Undo", "Delete guide"), "");
                        ret = true;
                        desktop->getTool()->discard_delayed_snap_event();
                        desktop->getTool()->use_tool_cursor();
                    }
                    break;
                case INK_KEY_Shift_L:
                case INK_KEY_Shift_R:
                    if (drag_type != SP_DRAG_MOVE_ORIGIN) {
                        Inkscape::set_svg_cursor(*desktop->getCanvas(), "rotate.svg");
                        ret = true;
                        break;
                    }
                default:
                    break;
            }
        },

        [&] (Inkscape::KeyReleaseEvent const &event) {
            switch (Inkscape::UI::Tools::get_latin_keyval(event)) {
                case INK_KEY_Shift_L:
                case INK_KEY_Shift_R: {
                    desktop->getCanvas()->set_cursor(Qt::OpenHandCursor);
                    break;
                }
                default:
                    break;
            }
        },

        [&] (Inkscape::CanvasEvent const &event) {}
    );

    return ret;
}

void sp_dt_guide_begin_drag(SPGuide* guide, Inkscape::UI::Widget::Canvas* canvas, int type) {
    if (!guide || !canvas) return;

    // Look up the canvas-item guide line for this canvas (friend access to SPGuide::views).
    auto guide_item = guide->findGuide(canvas);
    if (!guide_item) return;

    ruler_initiated = true;
    drag_type = static_cast<SPGuideDragType>(type);
    guide_moved = false;
    guide_item->grab(EventType::BUTTON_RELEASE | EventType::BUTTON_PRESS | EventType::MOTION);
}

static constexpr bool DEBUG_TOOL_SWITCHER = false;

static std::unordered_map<std::string, std::string> name_to_tool;
static std::string last_name;
static Inkscape::InputSource last_source = Inkscape::InputSource::MOUSE;

static void init_extended()
{
    // Enumerate Qt input devices and map tablet tools to Inkscape tools.
    const auto devices = QInputDevice::devices();
    for (const auto &dev : devices) {
        if (!dev) continue;

        auto const src = dev->type();
        if (src == QInputDevice::DeviceType::Mouse) continue;

        auto const name = dev->name().toStdString();
        if (name.empty() || name == "pad") continue;

        // Set the initial tool for the device.
        switch (src) {
            case QInputDevice::DeviceType::Stylus:
            case QInputDevice::DeviceType::Airbrush:
                name_to_tool[name] = "Calligraphic";
                break;
            default:
                break;
        }
    }
}

// Switch tool based on device that generated event.
// For example, switch to Calligraphy or Eraser tool when using a Wacom tablet pen.
// Enabled in "Input" section of preferences dialog.
void snoop_extended(Inkscape::CanvasEvent const &event, SPDesktop *desktop)
{
    // Restrict to events we're interested in.
    switch (event.type()) {
        case EventType::MOTION:
        case EventType::BUTTON_PRESS:
        case EventType::BUTTON_RELEASE:
        case EventType::SCROLL:
            break; // Good
        default:
            return;
    }

    // Extract information about the device of the event.
    auto device = event.device.get();
    if (!device) {
        // Not all event structures include a device field but the above should!
        std::cerr << "snoop_extended: missing source device! " << (int)event.type() << std::endl;
        return;
    }

    auto const &source = device->source;
    auto const &name = device->name;

    if (name.empty()) {
        std::cerr << "snoop_extended: name empty!" << std::endl;
        return;
    } else if (source == last_source && name == last_name) {
        // Device has not changed.
        return;
    }

    if constexpr (DEBUG_TOOL_SWITCHER) {
        std::cout << "Changed device: " << last_name << " -> " << name << std::endl;
    }

    // Save the tool currently selected for next time the device shows up.
    if (auto it = name_to_tool.find(last_name); it != name_to_tool.end()) {
        it->second = desktop->getActiveTool();
    }

    // Select the tool that was selected last time the device was seen.
    if (auto it = name_to_tool.find(name); it != name_to_tool.end()) {
        set_active_tool(desktop, it->second);
    }

    last_name = name;
    last_source = source;
}
