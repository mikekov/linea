// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *  @brief View helper class used by hatch, clippath, mask and pattern.
 *
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2023 the Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_OBJECT_VIEW_H
#define SEEN_OBJECT_VIEW_H

#include <2geom/rect.h>

#include "display/drawing-item-ptr.h"

template <typename DrawingItemType>
struct ObjectView
{
    DrawingItemPtr<DrawingItemType> drawingitem;
    Geom::OptRect bbox;
    unsigned key;
    ObjectView(DrawingItemPtr<DrawingItemType> drawingitem, Geom::OptRect const &bbox, unsigned key)
        : drawingitem{std::move(drawingitem)}
        , bbox{bbox}
        , key{key}
    {}
};

#endif
