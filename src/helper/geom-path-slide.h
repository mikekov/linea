// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_HELPER_GEOM_PATH_SLIDE_H
#define INKSCAPE_HELPER_GEOM_PATH_SLIDE_H

/**
 * @file
 * Geometry for sliding a junction node along a frozen path.
 */
/*
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <optional>
#include <2geom/path.h>
#include <2geom/point.h>

/// New handle positions for sliding the junction node of a frozen one- or
/// two-segment path along it. Positions are absolute; unset entries stay unchanged.
struct PathSlideHandles {
    enum class Collapse { None, Prev, Next };
    std::optional<Geom::Point> prev_front; ///< front handle of the previous node
    std::optional<Geom::Point> back;       ///< back handle of the slid node
    std::optional<Geom::Point> front;      ///< front handle of the slid node
    std::optional<Geom::Point> next_back;  ///< back handle of the next node
    Collapse collapse = Collapse::None;    ///< side that shrank to zero length at a slide extreme
};

/** Recompute the surrounding handles after a confine-to-path slide so the
 * node's move does not deform the path. @a new_pos is the node's target
 * position on the frozen segment path, and @a pos locates the split point
 * on it. The side of the frozen path containing the split gives exact
 * control points through subdivision; the other side, which spans the
 * rest of the split segment plus the remaining one, is covered by a
 * single cubic that blends a shape-preserving fit with a transport of
 * the original segment, depending on how smooth the junction at the node is.
 */
PathSlideHandles slide_node_along_path(const Geom::Path& path, const Geom::PathTime& pos, const Geom::Point& new_pos,
                                       const std::optional<Geom::Point>& prev_pos,
                                       const std::optional<Geom::Point>& next_pos);

#endif // INKSCAPE_HELPER_GEOM_PATH_SLIDE_H
