// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape Units internal linking
 *
 * Copyright (C) 2026 AUTHORS
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "util/units.h"
#include "io/resource.h"

namespace Inkscape::Util {

std::string UnitTable::getUnitsFilename()
{
    using namespace Inkscape::IO::Resource;
    return get_filename(UIS, "units.xml", false, true);
}

} // namespace Inkscape::Util
