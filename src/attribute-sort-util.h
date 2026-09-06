// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors:
 * see git history
 * Tavmjong Bah
 *
 * Copyright (C) 2016 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_ATTRIBUTE_SORT_UTIL_H
#define SEEN_ATTRIBUTE_SORT_UTIL_H

#include "xml/node.h"

using Inkscape::XML::Node;

/**
 * Utility functions for sorting attributes.
 */

/**
 * Sort attributes and CSS properties by name.
 */
void sp_attribute_sort_tree(Node& repr);

#endif // SEEN_ATTRIBUTE_SORT_UTIL_H
