// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2010 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_MEDIA_H
#define INKSCAPE_MEDIA_H

class Media {
public:
    bool print;
    bool screen;
};

void media_clear_all(Media &);
void media_set_all(Media &);

#endif /* !INKSCAPE_MEDIA_H */
