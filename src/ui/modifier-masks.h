// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_MODIFIER_MASKS_H
#define INKSCAPE_UI_MODIFIER_MASKS_H

/**
 * @file
 * Toolkit-agnostic modifier mask constants.
 *
 * These values mirror the historical GDK mask values so that existing
 * tool code and the Qt event converter (which already emits these) remain
 * binary-compatible without any behavioural change.
 */

// Keyboard modifier masks (match GDK definitions)
constexpr int INK_SHIFT_MASK   = 1 << 0;
constexpr int INK_CONTROL_MASK = 1 << 2;
constexpr int INK_ALT_MASK     = 1 << 3;
constexpr int INK_MOD2_MASK    = 1 << 4;  // Num Lock
constexpr int INK_SUPER_MASK   = 1 << 26;
constexpr int INK_HYPER_MASK   = 1 << 27;
constexpr int INK_META_MASK    = 1 << 28;

// Mouse button state masks (match GDK definitions)
constexpr int INK_BUTTON1_MASK = 1 << 8;
constexpr int INK_BUTTON2_MASK = 1 << 9;
constexpr int INK_BUTTON3_MASK = 1 << 10;
constexpr int INK_BUTTON4_MASK = 1 << 11;
constexpr int INK_BUTTON5_MASK = 1 << 12;

/// All keyboard modifiers used by Inkscape.
constexpr int INK_MODIFIER_MASK = INK_SHIFT_MASK | INK_CONTROL_MASK | INK_ALT_MASK | INK_META_MASK;

#endif // INKSCAPE_UI_MODIFIER_MASKS_H
