// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Text node implementation
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_XML_TEXT_NODE_H
#define SEEN_INKSCAPE_XML_TEXT_NODE_H

#include <glib.h>
#include "xml/simple-node.h"

namespace Inkscape {

namespace XML {

/**
 * @brief Text node, e.g. "Some text" in &lt;group&gt;Some text&lt;/group&gt;
 */
struct TextNode : public SimpleNode {
    TextNode(Util::ptr_shared content, Document *doc)
    : SimpleNode(g_quark_from_static_string("string"), doc)
    {
        setContent(content);
        _is_CData = false;
    }
    TextNode(Util::ptr_shared content, Document *doc, bool is_CData)
    : SimpleNode(g_quark_from_static_string("string"), doc)
    {
        setContent(content);
        _is_CData = is_CData;
    }
    TextNode(TextNode const &other, Document *doc)
    : SimpleNode(other, doc) {
      _is_CData = other._is_CData;
    }

    Inkscape::XML::NodeType type() const override { return Inkscape::XML::NodeType::TEXT_NODE; }
    bool is_CData() const { return _is_CData; }

protected:
    SimpleNode *duplicate(Document *doc) const override { return new TextNode(*this, doc); }
    bool _is_CData;
};

}

}

#endif
