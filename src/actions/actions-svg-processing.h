// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for processing svg documents
 *
 * Copyright (C) 2002-2023 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_PROCESSING_H
#define INK_ACTIONS_PROCESSING_H

class SPDocument;
namespace Inkscape {
    namespace XML {
        class Node;
    }
}

void add_actions_processing(SPDocument* doc);

void insert_text_fallback(Inkscape::XML::Node *repr, const SPDocument *original_doc, Inkscape::XML::Node *defs = nullptr);

#endif // INK_ACTIONS_PROCESSING_H
