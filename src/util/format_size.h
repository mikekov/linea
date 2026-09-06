// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Util::format_size - format a number into a byte display
 *
 * Copyright (C) 2005-2022 Inkscape Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_UTIL_FORMAT_SIZE_H
#define SEEN_INKSCAPE_UTIL_FORMAT_SIZE_H

#include <glibmm/main.h>
#include <glibmm/ustring.h>

namespace Inkscape {
namespace Util {

Glib::ustring format_size(std::size_t value);

Glib::ustring format_file_size(std::size_t value);

}}
#endif
