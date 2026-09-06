// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Theme — helper functions for UI theming and styling.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_THEME_H
#define LINEA_UI_THEME_H

#include <QColor>
#include <QPalette>

namespace Linea::UI {

// Prepare/alter light/dark theme palettes
void setLightThemePalette(QPalette& palette);
void setDarkThemePalette(QPalette& palette);

/**
 * Get the current theme's background color.
 */
QColor getBackgroundColor();

/**
 * Get the current theme's foreground color.
 */
QColor getForegroundColor();

/**
 * Get the current theme's accent color.
 */
QColor getAccentColor();

/**
 * Check if the current theme is dark.
 */
bool isDarkTheme();

/**
 * Set the icon theme (light or dark) for runtime switching.
 * Loads the appropriate .rcc resource file.
 * @param dark true for dark theme, false for light theme
 */
void setIconTheme(bool dark);

/**
 * Apply palette, icons and stylesheet for the given dark/light mode.
 * @param dark       true for dark theme, false for light theme
 * @param followSystem if true, do not pin QStyleHints::colorScheme so that
 *                   the system can continue emitting colorSchemeChanged.
 */
void setApplicationTheme(bool dark, bool followSystem = false);

} // namespace Linea::UI

#endif // LINEA_UI_THEME_H
