// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Common utility functions for manipulating style.
 *//*
 * Authors:
 * see git history
 * Shlomi Fish <shlomif@cpan.org>
 *
 * Copyright (C) 2016 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_DIALOGS_STYLE_UTILS_H
#define SEEN_DIALOGS_STYLE_UTILS_H

#include "desktop-style.h"

namespace Inkscape {
    inline bool is_query_style_updateable(const int style) {
        return (style == QUERY_STYLE_MULTIPLE_DIFFERENT || style == QUERY_STYLE_NOTHING);
    }
} // namespace Inkscape

#endif // SEEN_DIALOGS_STYLE_UTILS_H
