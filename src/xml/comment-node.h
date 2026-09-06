// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Comment node implementation
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Copyright 2005 MenTaLguY <mental@rydia.net>
 * 
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#ifndef SEEN_INKSCAPE_XML_COMMENT_NODE_H
#define SEEN_INKSCAPE_XML_COMMENT_NODE_H

#include <glib.h>
#include "xml/simple-node.h"

namespace Inkscape {

namespace XML {

/**
 * @brief Comment node, e.g. &lt;!-- Some comment --&gt;
 */
struct CommentNode : public SimpleNode {
    CommentNode(Util::ptr_shared content, Document *doc)
    : SimpleNode(g_quark_from_static_string("comment"), doc)
    {
        setContent(content);
    }

    CommentNode(CommentNode const &other, Document *doc)
    : SimpleNode(other, doc) {}

    Inkscape::XML::NodeType type() const override { return Inkscape::XML::NodeType::COMMENT_NODE; }

protected:
    SimpleNode *duplicate(Document *doc) const override { return new CommentNode(*this, doc); }
};

}

}

#endif
