// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief SVG tile filter effect
 *//*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SP_FETILE_H_SEEN
#define SP_FETILE_H_SEEN

#include "sp-filter-primitive.h"

class SPFeTile final
    : public SPFilterPrimitive
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

protected:
    std::unique_ptr<Inkscape::Filters::FilterPrimitive> build_renderer(Inkscape::DrawingItem *item) const override;
};

#endif // SP_FETILE_H_SEEN
