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
class LineaApplication;
namespace Inkscape {
    namespace XML {
        class Node;
    }
}

void add_actions_svg_processing(LineaApplication* app);

// Run a processing action by id against a specific document. Unlike the
// registered QActions (which act on the active document), extensions call this
// on the document they are processing. Returns false if the id is unknown.
bool run_svg_processing_action(SPDocument* doc, const char* name);

void insert_text_fallback(Inkscape::XML::Node *repr, const SPDocument *original_doc, Inkscape::XML::Node *defs = nullptr);

#endif // INK_ACTIONS_PROCESSING_H
