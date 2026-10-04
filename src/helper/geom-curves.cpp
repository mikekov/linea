// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Specific curve type functions for Inkscape, not provided by lib2geom.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "helper/geom-curves.h"

#include <algorithm>
#include <cmath>
#include <2geom/bezier-curve.h>

bool is_degenerate_handle(const Geom::Curve& curve, bool at_start) {
    if (auto cubic = dynamic_cast<const Geom::CubicBezier*>(&curve)) {
        const auto cps = cubic->controlPoints();
        const Geom::Point arm = cps[at_start ? 1 : 2] - cps[at_start ? 0 : 3];
        const Geom::Point chord = cps[3] - cps[0];
        if (arm.length() < Geom::EPSILON || chord.length() < Geom::EPSILON) return true;
        // A handle lying on the chord contributes no corner; a segment that
        // renders straight must not grow a real handle in the fit.
        return std::abs(Geom::cross(chord, arm)) / chord.length() < 0.01 * chord.length();
    }
    return true; // non-cubic segments carry no handles at all
}

bool has_degenerate_handles(const Geom::Curve& curve) {
    return is_degenerate_handle(curve, true) && is_degenerate_handle(curve, false);
}

Geom::Point inner_control_point(const Geom::Curve& curve, bool at_start) {
    if (auto cubic = dynamic_cast<const Geom::CubicBezier*>(&curve)) {
        return cubic->controlPoints()[at_start ? 1 : 2];
    }
    return at_start ? curve.initialPoint() : curve.finalPoint();
}

bool fit_cubic_to_curves(const std::vector<const Geom::Curve*>& input, const CubicFitEnd& start, const CubicFitEnd& end,
                         Geom::Point result[4]) {
    std::vector<const Geom::Curve*> chain;
    for (const Geom::Curve* curve : input) {
        if (curve && !curve->isDegenerate()) {
            chain.push_back(curve);
        }
    }
    if (chain.empty()) return false;

    // A single curve needs no fitting: reuse it verbatim so the result is
    // exactly the original geometry rather than an approximation of it,
    // honoring fixed end overrides.
    if (chain.size() == 1) {
        if (auto cubic = dynamic_cast<const Geom::CubicBezier*>(chain[0])) {
            const auto cps = cubic->controlPoints();
            std::copy(cps.begin(), cps.end(), result);
        } else {
            result[0] = result[1] = chain[0]->initialPoint();
            result[2] = result[3] = chain[0]->finalPoint();
        }
        if (start.fixed) {
            result[1] = *start.fixed;
        }
        if (end.fixed) {
            result[2] = *end.fixed;
        }
        return true;
    }

    const Geom::Point p0 = chain.front()->initialPoint();
    const Geom::Point p3 = chain.back()->finalPoint();
    const double chord = Geom::distance(p0, p3);
    if (chord < Geom::EPSILON) return false;

    // Inner control points when not solved: a fixed override, else the end
    // node itself (degenerate handle).
    const Geom::Point F1 = start.fixed ? *start.fixed : p0;
    const Geom::Point F2 = end.fixed ? *end.fixed : p3;
    const bool solve1 = !start.fixed && start.dir;
    const bool solve2 = !end.fixed && end.dir;

    Geom::Point t0, t1;
    const Geom::Point dir = (p3 - p0) / chord;
    if (solve1) {
        t0 = start.dir->length() > Geom::EPSILON ? *start.dir : dir;
    }
    if (solve2) {
        t1 = end.dir->length() > Geom::EPSILON ? *end.dir : dir;
    }

    // Parameterize samples by each curve's native parameter, giving curves
    // spans proportional to their chord lengths. When one piece shrinks to
    // a sliver this converges on the other piece's own parameterization,
    // so the fit reproduces that piece exactly and crossing the split's
    // original position does not jump. A chord-length parameterization
    // would not reproduce a piece's control points even for a single cubic.
    double extent = 0;
    for (const Geom::Curve* curve : chain) {
        extent += Geom::distance(curve->initialPoint(), curve->finalPoint());
    }
    if (extent < Geom::EPSILON) return false;

    std::vector<Geom::Point> pts;
    std::vector<double> tk;
    int constexpr samples = 16;
    double offset = 0;
    for (const Geom::Curve* curve : chain) {
        const double len = Geom::distance(curve->initialPoint(), curve->finalPoint());
        for (int s = 1; s <= samples; ++s) { // skip the duplicated shared endpoint
            const double t = static_cast<double>(s) / samples;
            pts.push_back(curve->pointAt(t));
            tk.push_back((offset + len * t) / extent);
        }
        offset += len;
    }

    // Minimize sum |base(t) + l1*B1(t)*t0 - l2*B2(t)*t1 - Q(t)|^2 where
    // base(t) is the cubic with the fixed inner control points in place.
    double a = 0, b = 0, c = 0, e1 = 0, e2 = 0;
    for (size_t k = 0; k < pts.size(); ++k) {
        const double t = tk[k];
        const double mt = 1 - t;
        const double b1 = 3 * mt * mt * t;
        const double b2 = 3 * mt * t * t;
        const Geom::Point d = pts[k] - (mt * mt * mt * p0 + b1 * F1 + b2 * F2 + t * t * t * p3);
        a += b1 * b1;
        b += b2 * b2;
        c += b1 * b2 * (solve1 && solve2 ? Geom::dot(t0, t1) : 0);
        e1 += solve1 ? b1 * Geom::dot(t0, d) : 0;
        e2 -= solve2 ? b2 * Geom::dot(t1, d) : 0;
    }

    // det vanishes when the end tangents are near-parallel; the chord
    // approximation is the natural limit of the solve in that case.
    const double det = a * b - c * c;
    double l1 = 0, l2 = 0;
    if (solve1 && solve2) {
        if (det < 1e-3 * a * b) {
            l1 = l2 = chord / 3;
        } else {
            l1 = (e1 * b + c * e2) / det;
            l2 = (a * e2 + c * e1) / det;
        }
    } else if (solve1) {
        l1 = e1 / a;
    } else if (solve2) {
        l2 = e2 / b;
    }
    // A negative length flips the handle against its given direction and
    // guarantees a loop, so lengths are floored at zero.
    const double limit = 4 * chord;

    result[0] = p0;
    result[1] = solve1 ? p0 + std::clamp(l1, 0.0, limit) * t0 : F1;
    result[2] = solve2 ? p3 - std::clamp(l2, 0.0, limit) * t1 : F2;
    result[3] = p3;

    return true;
}
