// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <glib.h>
#include "livarot/sweep-tree.h"
#include "livarot/sweep-tree-list.h"


SweepTreeList::SweepTreeList(int s) :
    nbTree(0),
    maxTree(s),
    trees((SweepTree *) g_malloc(s * sizeof(SweepTree))),
    racine(nullptr)
{
    /* FIXME: Use new[] for trees initializer above, but watch out for bad things happening when
     * SweepTree::~SweepTree is called.
     */
}


SweepTreeList::~SweepTreeList()
{
    g_free(trees);
    trees = nullptr;
}


SweepTree *SweepTreeList::add(Shape *iSrc, int iBord, int iWeight, int iStartPoint, Shape */*iDst*/)
{
    if (nbTree >= maxTree) {
        return nullptr;
    }

    int const n = nbTree++;
    trees[n].MakeNew(iSrc, iBord, iWeight, iStartPoint);

    return trees + n;
}
