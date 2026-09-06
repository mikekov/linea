// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Manages external resources such as image and css files.
 *
 * Copyright 2011  Jon A. Cruz  <jon@joncruz.org>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <string>
#include <vector>

class SPDocument;

namespace Inkscape {

std::string optimizePath(std::string const &path, std::string const &base, unsigned int parents = 2);
bool fixBrokenLinks(SPDocument *doc);

}
