// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_STREQ_H
#define INKSCAPE_STREQ_H

#include <cstring>

/** Convenience/readability wrapper for strcmp(a,b)==0. */
inline bool
streq(char const *a, char const *b)
{
    return std::strcmp(a, b) == 0;
}

struct streq_rel {
    bool operator()(char const *a, char const *b) const
    {
        return (std::strcmp(a, b) == 0);
    }
};

#endif /* !INKSCAPE_STREQ_H */
