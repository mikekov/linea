// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorWheel — abstract interface for a color wheel/plate widget
 */

#ifndef LINEA_UI_COLOR_WHEEL_H
#define LINEA_UI_COLOR_WHEEL_H

#include <sigc++/connection.h>
#include <sigc++/slot.h>
#include "colors/color.h"

class QWidget;

namespace Linea::UI {

class ColorWheel {
public:
    virtual ~ColorWheel() = default;

    // set current color
    virtual void setColor(const Inkscape::Colors::Color& color) = 0;

    // connect a callback invoked when the user picks a color; returns a blockable connection
    virtual sigc::connection connectColorChanged(sigc::slot<void(const Inkscape::Colors::Color&)> cb) = 0;

    // get the underlying QWidget
    virtual QWidget* getWidget() = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_WHEEL_H
