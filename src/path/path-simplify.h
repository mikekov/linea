// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Simplify paths (reduce node count).
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef PATH_SIMPLIFY_H
#define PATH_SIMPLIFY_H

class SPItem;

int path_simplify(SPItem *item, float threshold, bool justCoalesce, double size);

#endif // PATH_SIMPLIFY_H
