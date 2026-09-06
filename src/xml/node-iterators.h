// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Node iterators
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2004 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_XML_SP_REPR_ITERATORS_H
#define SEEN_INKSCAPE_XML_SP_REPR_ITERATORS_H

#include "util/forward-pointer-iterator.h"

namespace Inkscape {
namespace XML {

class Node;

struct NodeSiblingIteratorStrategy {
    static Node const *next(Node const *node);
};

struct NodeParentIteratorStrategy {
    static Node const *next(Node const *node);
};

typedef Inkscape::Util::ForwardPointerIterator<Node,
                                               NodeSiblingIteratorStrategy>
        NodeSiblingIterator;

typedef Inkscape::Util::ForwardPointerIterator<Node const,
                                               NodeSiblingIteratorStrategy>
        NodeConstSiblingIterator;

typedef Inkscape::Util::ForwardPointerIterator<Node,
                                               NodeParentIteratorStrategy>
        NodeParentIterator;

typedef Inkscape::Util::ForwardPointerIterator<Node const,
                                               NodeParentIteratorStrategy>
        NodeConstParentIterator;

}
}

#endif
