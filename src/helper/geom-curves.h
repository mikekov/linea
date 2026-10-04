// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_HELPER_GEOM_CURVES_H
#define INKSCAPE_HELPER_GEOM_CURVES_H

/**
 * @file
 * Specific curve type functions for Inkscape, not provided by lib2geom.
 */
/*
 * Author:
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *
 * Copyright (C) 2008-2009 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>
#include <2geom/curve.h>
#include <2geom/line.h>
#include <2geom/bezier-curve.h>
#include <2geom/point.h>

/// \todo un-inline this function
inline bool is_straight_curve(Geom::BezierCurve const &c)
{
    // the curve can be a quad/cubic bezier, but could still be a perfect straight line
    // if the control points are exactly on the line connecting the initial and final points.
    auto const line = Geom::Line{c.initialPoint(), c.finalPoint()};
    for (int i = 1; i < c.order(); i++) {
        if (!Geom::are_near(c[i], line)) {
            return false;
        }
    }
    return true;
}

inline bool is_straight_curve(Geom::Curve const &c)
{
    if (dynamic_cast<Geom::LineSegment const *>(&c)) {
        return true;
    } else if (auto bezier = dynamic_cast<Geom::BezierCurve const *>(&c)) {
        return is_straight_curve(*bezier);
    } else {
        return false;
    }
}

/** Whether the curve's inner control point at the given end is
 * degenerate, i.e. the node it belongs to has no handle on that side.
 */
bool is_degenerate_handle(const Geom::Curve& curve, bool at_start);

/** Whether the curve renders as a straight line: both arms lie on the chord. */
bool has_degenerate_handles(const Geom::Curve& curve);

/** The curve's inner control point at the given end: the absolute
 * handle position of the node it belongs to.
 */
Geom::Point inner_control_point(const Geom::Curve& curve, bool at_start);

/** One end of a cubic fit: the inner control point is either @a fixed at an
 * absolute position or left free to slide along @a dir from its end node.
 */
struct CubicFitEnd {
    const Geom::Point* fixed = nullptr;
    const Geom::Point* dir = nullptr;
};

/** Fit a single cubic bezier to a chain of curves. Each inner handle is
 * either fixed in place or constrained to a direction so that only its
 * length is unknown - lengths are solved in closed form by least squares
 * over sampled points. Being deterministic and smooth in the split
 * position, it does not jiggle the way an iterative fitter does across
 * drag moves.
 */
bool fit_cubic_to_curves(const std::vector<const Geom::Curve*>& input, const CubicFitEnd& start, const CubicFitEnd& end,
                         Geom::Point result[4]);

#endif // INKSCAPE_HELPER_GEOM_CURVES_H
