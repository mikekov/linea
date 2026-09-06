// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2010 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "media.h"

void
media_clear_all(Media &media)
{
    media.print = false;
    media.screen = false;
}

void
media_set_all(Media &media)
{
    media.print = true;
    media.screen = true;
}
