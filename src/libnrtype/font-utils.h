// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Font selection utilities.
 */
/*
 * Authors:
 *   See Git history
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LIBNRTYPE_FONT_UTILS_H
#define LIBNRTYPE_FONT_UTILS_H

#include <map>
#include <string>
#include <vector>

#include <glibmm/ustring.h>

namespace Inkscape {

// Pass fontspec to and back from Pango to get a the fontspec in canonical form.
Glib::ustring canonize_fontspec(Glib::ustring const &fontspec);

// Returns a map of 'tag' => 'value' from a variations string.
std::map<std::string, std::string> parse_variations(const char* variations);

} // namespace Inkscape

#endif // LIBNRTYPE_FONT_UTILS_H
