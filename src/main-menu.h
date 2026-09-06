// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main menu bar construction
 */

#ifndef LINEA_MAIN_MENU_H
#define LINEA_MAIN_MENU_H

class QMenuBar;

/**
 * Populate the given menu bar with the application's main menus
 * (File, Edit, View, ...), pulling actions from ActionRegistry.
 */
void createMainMenu(QMenuBar* bar);

#endif // LINEA_MAIN_MENU_H
