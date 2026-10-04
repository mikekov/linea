// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Geometry for sliding a junction node along a frozen path.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "helper/geom-path-slide.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "helper/geom-curves.h"

namespace {

// The corner this node makes in the frozen path is transported with it:
// the arm on the far side of the node's original position keeps its
// angle to the local tangent. alpha is the signed angle from the back
// arm to the front arm at the junction; for smooth nodes it is ~180
// degrees and the transported arm coincides with the local tangent, so
// smooth and corner nodes are handled by the same code.
double junction_angle(const Geom::Path& path) {
    double alpha = M_PI;
    if (path.size() >= 2) {
        const Geom::Point tin = path[0].unitTangentAt(1);
        const Geom::Point tout = path[1].unitTangentAt(0);
        if (tin.length() > Geom::EPSILON && tout.length() > Geom::EPSILON) {
            const Geom::Point back_arm = -tin;
            alpha = std::atan2(Geom::cross(back_arm, tout), Geom::dot(back_arm, tout));
        }
    }
    return alpha;
}

// The side containing the split is an exact portion of the frozen
// curve. A straight segment stays straight with its handles retracted;
// a curved one takes the subdivision's control points verbatim, even
// where a handle was retracted before, since the node has to trace it.
std::pair<Geom::Point, Geom::Point> exact_side_handles(const Geom::Curve& portion, bool straight, const Geom::Point& s,
                                                       const Geom::Point& e) {
    return {straight ? s : inner_control_point(portion, true), straight ? e : inner_control_point(portion, false)};
}

// The far side is a single cubic from the node to its far neighbor.
// Two candidates share the same end directions and differ only in
// handle lengths: a least-squares fit to the frozen chain, which keeps
// the path shape but is only meaningful across a smooth junction, and
// a transport of the original far segment, whose handle lengths scale
// with the chord and whose arm at the moved end is rotated so the
// corner angle is kept. @a moved_dir is the fit direction at the moved
// end (front_arm when the node starts the segment, back_in when it ends it).
std::optional<std::pair<Geom::Point, Geom::Point>> far_side_handles(const std::vector<const Geom::Curve*>& chain,
                                                                    const Geom::Curve& original, bool moved_is_start,
                                                                    const Geom::Point& moved_dir, const Geom::Point& s,
                                                                    const Geom::Point& e, double smooth_w) {
    const bool straight = has_degenerate_handles(original);
    const Geom::Point t0 = moved_is_start ? moved_dir : original.unitTangentAt(0);
    const Geom::Point t1 = moved_is_start ? original.unitTangentAt(1) : moved_dir;
    const CubicFitEnd s0 = straight ? CubicFitEnd{&s, nullptr} : CubicFitEnd{nullptr, &t0};
    const CubicFitEnd s1 = straight ? CubicFitEnd{&e, nullptr} : CubicFitEnd{nullptr, &t1};
    Geom::Point fit[4];
    if (!fit_cubic_to_curves(chain, s0, s1, fit)) return std::nullopt;

    const double old_chord = Geom::distance(original.initialPoint(), original.finalPoint());
    const double ratio = old_chord > Geom::EPSILON ? Geom::distance(s, e) / old_chord : 1.0;
    const double len0 = straight ? 0.0 : Geom::distance(inner_control_point(original, true), original.initialPoint());
    const double len1 = straight ? 0.0 : Geom::distance(inner_control_point(original, false), original.finalPoint());
    const Geom::Point tr1 = s + len0 * ratio * t0;
    const Geom::Point tr2 = e - len1 * ratio * t1;

    return std::pair{smooth_w * fit[1] + (1 - smooth_w) * tr1, smooth_w * fit[2] + (1 - smooth_w) * tr2};
}

} // namespace

PathSlideHandles slide_node_along_path(const Geom::Path& path, const Geom::PathTime& pos, const Geom::Point& new_pos,
                                       const std::optional<Geom::Point>& prev_pos,
                                       const std::optional<Geom::Point>& next_pos) {
    PathSlideHandles result;
    if (pos.curve_index >= path.size()) return result;
    const bool in_prev = pos.curve_index == 0 && prev_pos;
    if (in_prev ? (next_pos && path.size() < 2) : !next_pos) return result;

    const Geom::Coord u = pos.t;
    const Geom::Curve* curve = &path[pos.curve_index];

    const double alpha = junction_angle(path);
    const Geom::Point T = curve->unitTangentAt(u);
    const double ca = std::cos(alpha), sa = std::sin(alpha);
    const Geom::Point front_arm(-T.x() * ca + T.y() * sa, -T.x() * sa - T.y() * ca); // -T rotated by alpha
    const Geom::Point back_in(-T.x() * ca - T.y() * sa, T.x() * sa - T.y() * ca);    // -T rotated by -alpha

    // How smooth the junction is decides between preserving the path shape
    // (a smooth node slides along the path without deforming it) and
    // transporting the corner (a cusp cannot slide without moving its
    // corner, so the far segment is reshaped instead). The deflection at
    // the junction decides, not the node type, since many tools emit
    // NODE_CUSP for nearly smooth nodes; the weight is continuous so the
    // two regimes do not jump.
    const double smooth_w = std::clamp((std::abs(alpha) - 0.55 * M_PI) / (0.3 * M_PI), 0.0, 1.0);

    // At the slide extremes a side collapses to zero length. Its handles
    // are set degenerate so the collapsed segment renders as nothing;
    // if the drag ends there the nodes fuse instead (see ungrabbed()).
    double constexpr extreme = 1e-6;
    if (in_prev) {
        // Split is in prev->this. The near side is an exact portion; the far
        // side must cover the rest of this segment plus this->next.
        if (u > extreme) {
            std::unique_ptr<Geom::Curve> left{curve->portion(0, u)};
            const auto [f, b] = exact_side_handles(*left, has_degenerate_handles(*curve), *prev_pos, new_pos);
            result.prev_front = f;
            result.back = b;
        } else {
            result.prev_front = *prev_pos;
            result.back = new_pos;
            result.collapse = PathSlideHandles::Collapse::Prev;
        }
        if (next_pos) {
            std::unique_ptr<Geom::Curve> rest{curve->portion(u, 1)};
            if (auto fb =
                    far_side_handles({rest.get(), &path[1]}, path[1], true, front_arm, new_pos, *next_pos, smooth_w)) {
                result.front = fb->first;
                result.next_back = fb->second;
            }
        }
    } else {
        // Split is in this->next (or the only segment when prev is absent).
        if (u < 1 - extreme) {
            std::unique_ptr<Geom::Curve> right{curve->portion(u, 1)};
            const auto [f, b] = exact_side_handles(*right, has_degenerate_handles(*curve), new_pos, *next_pos);
            result.front = f;
            result.next_back = b;
        } else {
            result.front = new_pos;
            result.next_back = *next_pos;
            result.collapse = PathSlideHandles::Collapse::Next;
        }
        if (prev_pos) {
            std::unique_ptr<Geom::Curve> rest{curve->portion(0, u)};
            if (auto fb =
                    far_side_handles({&path[0], rest.get()}, path[0], false, back_in, *prev_pos, new_pos, smooth_w)) {
                result.prev_front = fb->first;
                result.back = fb->second;
            }
        }
    }
    return result;
}
