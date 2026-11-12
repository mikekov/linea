// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * createColorClipboardMenu — Copy/Paste color menu bound to a ColorHolder.
 */

#ifndef LINEA_UI_COLOR_CLIPBOARD_MENU_H
#define LINEA_UI_COLOR_CLIPBOARD_MENU_H

#include <functional>
#include <memory>

class QMenu;
class QWidget;

#include "colors/spaces/enum.h"

namespace Linea::UI {

class ColorEntry;
class ColorHolder;

/**
 * Build a menu with Copy/Paste actions operating on `colors`, parented to
 * `parent`. Colors cross the clipboard as CSS text so they also work across
 * applications. Reusable wherever a ColorHolder exists (color pickers, paint
 * indicators, ...).
 *
 * `display_space`, when set, is queried on each Copy and the color is
 * serialized in that space — pass the picker's current space so Copy matches
 * what the user sees. Without it Copy uses the color's own space.
 *
 * `entry`, when set, adds a "Show RGB Hex" / "Show CSS Color" radio pair that
 * switches the entry's display format.
 */
QMenu* createColorClipboardMenu(std::shared_ptr<ColorHolder> colors,
                                std::function<Inkscape::Colors::Space::Type()> display_space,
                                QWidget* parent = nullptr,
                                ColorEntry* entry = nullptr);
QMenu* createColorClipboardMenu(std::shared_ptr<ColorHolder> colors, QWidget* parent = nullptr);

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_CLIPBOARD_MENU_H
