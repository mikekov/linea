// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_EVENTS_INPUT_DEVICE_H
#define INKSCAPE_UI_WIDGET_EVENTS_INPUT_DEVICE_H

#include <string>

namespace Inkscape {

/**
 * The type of input device that sourced an event, mirroring
 * Gdk::InputSource.  Used for tablet-tool switching logic.
 */
enum class InputSource {
    MOUSE,
    PEN,
    ERASER,
    TOUCHPAD,
    TRACKPOINT,
    TABLET_PAD,
    TOUCHSCREEN,
    UNKNOWN
};

/**
 * Toolkit-agnostic descriptor of the device that sourced an event.
 * Replaces the old std::shared_ptr<Gdk::Device const> field on CanvasEvent.
 */
struct InputDevice {
    /// Human-readable device name (may be empty).
    std::string name;

    /// Category of the device.
    InputSource source = InputSource::UNKNOWN;
};

} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_EVENTS_INPUT_DEVICE_H
