// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Debug::timestamp - timestamp strings
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2007 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_DEBUG_TIMESTAMP_H
#define SEEN_INKSCAPE_DEBUG_TIMESTAMP_H

#include <memory>
#include <string>

namespace Inkscape {

namespace Debug {

std::shared_ptr<std::string> timestamp();

}

}

#endif
