// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Free functions for updating the paint indicator from desktop/selection state.
 */

#ifndef LINEA_UI_DESKTOP_UPDATE_PAINT_INDICATOR_H
#define LINEA_UI_DESKTOP_UPDATE_PAINT_INDICATOR_H

#include <optional>

#include "style-internal.h"

class SPDesktop;
class PaintIndicator;
class ColorPaletteWidget;

namespace Linea {
struct PresentationState;
struct PaintProp;
template <typename T>
class mixed_property;
} // namespace Linea

namespace Linea::UI {

/// Update paint indicator from explicit fill/stroke properties.
void updatePaintIndicator(PaintIndicator* p, const Linea::mixed_property<Linea::PaintProp>& fill,
                          const Linea::mixed_property<Linea::PaintProp>& stroke, const SPIPaintOrder& order);

/// Update paint indicator from the desktop's current tool style (when no selection).
void updatePaintIndicator(PaintIndicator* p, SPDesktop* desktop);

/// Refresh paint indicator and color palette from a presentation state.
void refreshPaintIndicator(PaintIndicator* p, ColorPaletteWidget* palette, SPDesktop* desktop,
                           const Linea::PresentationState& presentation);

} // namespace Linea::UI

#endif // LINEA_UI_DESKTOP_UPDATE_PAINT_INDICATOR_H
