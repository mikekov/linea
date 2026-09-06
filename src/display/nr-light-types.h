// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_NR_LIGHT_TYPES_H
#define SEEN_NR_LIGHT_TYPES_H

namespace Inkscape {
namespace Filters {

enum LightType
{
    NO_LIGHT = 0,
    DISTANT_LIGHT,
    POINT_LIGHT,
    SPOT_LIGHT
};

struct DistantLightData
{
    double azimuth, elevation;
};

struct PointLightData
{
    double x, y, z;
};

struct SpotLightData
{
    double x, y, z;
    double pointsAtX, pointsAtY, pointsAtZ;
    double limitingConeAngle;
    double specularExponent;
};

} // namespace Filters
} // namespace Inkscape

#endif // SEEN_NR_LIGHT_TYPES_H
