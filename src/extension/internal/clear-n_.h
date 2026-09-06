// SPDX-License-Identifier: GPL-2.0-or-later
/**
    \file clear-n_.h

    A way to clear the N_ macro, which is defined as an inline function.
    Unfortunately, this makes it so it is hard to use in static strings
    where you only want to translate a small part.  Including this
    turns it back into a a macro.
*/
/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *
 * Copyright (C) 2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef N_
#undef N_
#endif
#define N_(x) x

#ifdef NC_
#undef NC_
#endif
#define NC_(c, x) x
