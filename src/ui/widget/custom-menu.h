// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Helper to build a custom-styled popup menu with aligned shortcuts.
 *
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2026 Authors
 */

#ifndef LINEA_UI_WIDGET_CUSTOM_MENU_H
#define LINEA_UI_WIDGET_CUSTOM_MENU_H

#include <QMenu>
#include <functional>
#include <span>
#include <string_view>

namespace Linea::UI {

struct CustomMenuItem {
    std::string action;
    QString title;
    QString shortcut;
    QString description;
    QIcon icon;
    std::function<void()> onTrigger;
    QSize iconSize = QSize(16, 16);

    // Submenu control. When submenuStart is true this item opens a new submenu
    // (title is the submenu label); subsequent items are added to it until an
    // item with submenuEnd = true is reached. Nested submenus are supported.
    bool submenuStart = false;
    bool submenuEnd = false;
};

// Convert a globally registered action into a menu item. GTK-style action
// prefixes (app., win., doc., ctx.) are removed before registry lookup.
// An explicit label overrides the action's label.
CustomMenuItem actionToCustomMenuItem(std::string_view action_id, QString label = {});

// Append individual items while constructing a menu imperatively.
void appendCustomAction(QMenu* menu, const CustomMenuItem& item);
void appendCustomSeparator(QMenu* menu);
QMenu* appendCustomSubmenu(QMenu* menu, const QString& title);

QMenu* createCustomMenu(QWidget* parent, std::span<const char* const> action_ids, bool supportCheckMarks = true);

QMenu* createCustomMenu(QWidget* parent, std::span<const CustomMenuItem> items, bool supportCheckMarks);

// Populate an existing menu with custom items (used for dynamic menus that
// are cleared and rebuilt on each show).
void createCustomMenuInto(QMenu* menu, std::span<const CustomMenuItem> items, bool supportCheckMarks);

void installRightAlignedMenuFilter(QMenu* menu, int arrow_width = 16);

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_CUSTOM_MENU_H
