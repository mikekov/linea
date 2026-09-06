// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Icon utilities
 *
 * Copyright (C) 2025
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ICON_UTILITIES_H
#define INK_ICON_UTILITIES_H

#include <string>
#include <QIcon>

namespace Inkscape {

QIcon load_svg_icon(const std::string& file_name, int icon_size, double device_pixel_ratio = 1.0);

} // namespace Inkscape

#endif // INK_ICON_UTILITIES_H
