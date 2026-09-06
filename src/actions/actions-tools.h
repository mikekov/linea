// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for switching tools. Also includes functions to set and get active tool.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_TOOLS_H
#define INK_ACTIONS_TOOLS_H

#include <glibmm.h>
#include <2geom/point.h>

class LineaWindow;
class SPDesktop;
class SPItem;

// Returns the action ID (e.g. "tool-rect") for a tool name (e.g. "Rect").
std::string tool_action_id(std::string_view tool_name);

void open_tool_preferences(LineaWindow* win, Glib::ustring const &tool);

void set_active_tool(SPDesktop *desktop, Glib::ustring const &tool);
void set_active_tool(SPDesktop *desktop, SPItem *item, Geom::Point const p);

void recreate_active_tool(SPDesktop* desktop);

void tool_preferences(Glib::ustring const &tool, LineaWindow *win);

// Standard function to add actions.
void add_actions_tools(LineaWindow* win);

#endif // INK_ACTIONS_TOOLS_H
