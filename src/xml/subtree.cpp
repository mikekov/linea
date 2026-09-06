// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * XML::Subtree - proxy for an XML subtree
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Copyright 2005 MenTaLguY <mental@rydia.net>
 * 
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "xml/node.h"
#include "xml/subtree.h"
#include "xml/node-iterators.h"

namespace Inkscape {
namespace XML {

Subtree::Subtree(Node &root) : _root(root) {
    _root.addSubtreeObserver(_observers);
}

Subtree::~Subtree() {
    _root.removeSubtreeObserver(_observers);
}

namespace {

void synthesize_events_recursive(Node &node, NodeObserver &observer) {
    node.synthesizeEvents(observer);
    for ( NodeSiblingIterator iter = node.firstChild() ; iter ; ++iter ) {
        synthesize_events_recursive(*iter, observer);
    }
}

}

void Subtree::synthesizeEvents(NodeObserver &observer) {
    synthesize_events_recursive(_root, observer);
}

void Subtree::addObserver(NodeObserver &observer) {
    _observers.add(observer);
}

void Subtree::removeObserver(NodeObserver &observer) {
    _observers.remove(observer); 
}

}
}
