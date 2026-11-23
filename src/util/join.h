// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Join lists of elements, while respecting internationalization.
 *
 * Authors:
 * Copyright (C) 2026 Michael Terry
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UTIL_JOIN_H
#define INKSCAPE_UTIL_JOIN_H

#include <set>

#include <glibmm/ustring.h>

namespace Inkscape {
namespace Util {

Glib::ustring join_with_separator(std::set<Glib::ustring> const &items);

} // namespace Util
} // namespace Inkscape

#endif // define INKSCAPE_UTIL_JOIN_H
