// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Qt color wheel factory — creates a ColorWheel/ColorPlate for a given color space type.
 */

#ifndef LINEA_UI_COLOR_WHEEL_FACTORY_H
#define LINEA_UI_COLOR_WHEEL_FACTORY_H

#include "colors/spaces/enum.h"

class QWidget;

namespace Linea::UI {

class ColorWheel;

// Create a color wheel widget for the given space type and shape; null if unsupported.
ColorWheel* createColorWheel(Inkscape::Colors::Space::Type type, bool disc, QWidget* parent = nullptr);

// Returns true if a color wheel exists for this space type.
bool canCreateColorWheel(Inkscape::Colors::Space::Type type);

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_WHEEL_FACTORY_H
