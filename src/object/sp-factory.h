// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Factory for SPObject tree
 *
 * Authors:
 *   Markus Engel
 *
 * Copyright (C) 2013 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SP_FACTORY_SEEN
#define SP_FACTORY_SEEN

#include <string>

class SPObject;

namespace Inkscape {
namespace XML {
class Node;
}
}

struct SPFactory {
    static SPObject *createObject(std::string const &id);
    static bool supportsType(std::string const &id);
};

struct NodeTraits {
    static std::string get_type_string(Inkscape::XML::Node const &node);
};

#endif
