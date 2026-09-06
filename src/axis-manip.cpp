// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Generic auxiliary routines for 3D axes
 *
 * Authors:
 *   Maximilian Albert <Anhalter42@gmx.de>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h>
#include "axis-manip.h"

namespace Proj {

Axis axes[4]   = { X,  Y,  Z, W };

} // namespace Proj


namespace Box3D {

Axis axes[3]   = { X,  Y,  Z };
Axis planes[3] = { XY, XZ, YZ };
FrontOrRear face_positions [2] = { FRONT, REAR };

std::pair <Axis, Axis>
get_remaining_axes (Axis axis) {
    if (!is_single_axis_direction (axis)) return std::make_pair (NONE, NONE);
    Axis plane = orth_plane_or_axis (axis);
    return std::make_pair (extract_first_axis_direction (plane), extract_second_axis_direction (plane));
}

} // namespace Box3D
