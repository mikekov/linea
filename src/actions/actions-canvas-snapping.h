// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for toggling snapping preferences. Tied to a particular document.
 *
 * As preferences are stored per document, changes should be propagated to any window with same document.
 *
 * Copyright (C) 2019 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 */

#ifndef INK_ACTIONS_CANVAS_SNAPPING_H
#define INK_ACTIONS_CANVAS_SNAPPING_H

#include <vector>

namespace Glib {
class ustring;
} // namespace Glib

namespace Gio {
class ActionMap;
} // namespace Gio

class SPDocument;
class LineaWindow;

namespace Inkscape {
class SnapPreferences;
} // namespace Inkscape

void add_actions_canvas_snapping(Gio::ActionMap* map);
void add_actions_canvas_snapping(LineaWindow* win);
std::vector<std::vector<Glib::ustring>> get_extra_data_canvas_snapping();
Inkscape::SnapPreferences& get_snapping_preferences();
void transition_to_simple_snapping();
void apply_simple_snap_defaults(Gio::ActionMap &map);

#endif // INK_ACTIONS_CANVAS_SNAPPING_H
