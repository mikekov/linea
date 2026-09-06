// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Consolidates version info for Inkscape,
 * its various dependencies and the OS we're running on
 *//*
 * Authors:
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2021 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_VERSION_INFO_H
#define SEEN_INKSCAPE_VERSION_INFO_H

#include <string>

namespace Inkscape {

    std::string inkscape_version();
    std::string inkscape_revision();
    std::string os_version();
    std::string debug_info();

    unsigned short int inkscape_build_year();
} // namespace Inkscape

namespace Linea {

    std::string linea_version();
    std::string linea_version_full();

} // namespace Linea

#endif // SEEN_INKSCAPE_VERSION_INFO_H
