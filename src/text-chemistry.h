// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_TEXT_CHEMISTRY_H
#define SEEN_TEXT_CHEMISTRY_H

// TODO move to selection-chemistry?

/*
 * Text commands
 *
 * Authors:
 *   bulia byak
 *
 * Copyright (C) 2004 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <utility>
#include <vector>
#include <glibmm/ustring.h>

class SPDocument;
class SPDesktop;

void text_put_on_path(SPDesktop* desktop);
void text_remove_from_path(SPDesktop* desktop);
void text_remove_all_kerns(SPDesktop* desktop);
void text_flow_into_shape(SPDesktop* desktop);
void text_flow_shape_subtract(SPDesktop* desktop);
void text_unflow(SPDesktop* desktop);
void flowtext_to_text(SPDesktop* desktop);
void text_to_glyphs(SPDesktop* desktop);
enum text_ref_t { TEXT_REF_DEF = 0x1, TEXT_REF_EXTERNAL = 0x2, TEXT_REF_INTERNAL = 0x4, };
using text_refs_t = std::vector<std::pair<Glib::ustring, text_ref_t>>;
template<typename InIter>
text_refs_t text_categorize_refs(SPDocument *doc, InIter begin, InIter end, text_ref_t which);
template<typename InIterOrig, typename InIterCopy>
void text_relink_refs(text_refs_t const &refs,
        InIterOrig origBegin, InIterOrig origEnd, InIterCopy copyBegin);

#include "text-chemistry-impl.h"

#endif // SEEN_TEXT_CHEMISTRY_H
