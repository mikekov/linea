// SPDX-License-Identifier: GPL-2.0-or-later
/**
 *  @file src/snapper.cpp
 *  Snapper class.
 *
 *  Authors:
 *    Carl Hetherington <inkscape@carlh.net>
 *    Diederik van Lierop <mail@diedenrezi.nl>
 *
 *  Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h> // g_assert

#include "snapper.h"

/**
 *  Construct new Snapper for named view.
 *  @param nv Named view.
 *  @param d Snap tolerance.
 */
Inkscape::Snapper::Snapper(SnapManager *sm, Geom::Coord const /*t*/) :
    _snapmanager(sm),
    _snap_enabled(true),
    _snap_visible_only(true)
{
    g_assert(_snapmanager != nullptr);
}

/**
 *  @param s true to enable this snapper, otherwise false.
 */

void Inkscape::Snapper::setEnabled(bool s)
{
    _snap_enabled = s;
}

void Inkscape::Snapper::setSnapVisibleOnly(bool s)
{
    _snap_visible_only = s;
}
