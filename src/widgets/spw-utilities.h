// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape Widget Utilities
 *
 * Author:
 *   Bryce W. Harrington <brycehar@bryceharrington.org>
 *
 * Copyright (C) 2003 Bryce Harrington
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SPW_UTILITIES_H
#define SEEN_SPW_UTILITIES_H

#include <glibmm/ustring.h>

namespace Gtk {
class Widget;
} // namespace Gtk

/// Get string action target, if available.
Glib::ustring sp_get_action_target(Gtk::Widget* widget);

#endif // SEEN_SPW_UTILITIES_H
