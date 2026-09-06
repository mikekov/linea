// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Commit events.
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_UI_TOOL_COMMIT_EVENTS_H
#define SEEN_UI_TOOL_COMMIT_EVENTS_H

namespace Inkscape {
namespace UI {

/// This is used to provide sensible messages on the undo stack.
enum CommitEvent {
    COMMIT_MOUSE_MOVE,
    COMMIT_KEYBOARD_MOVE_X,
    COMMIT_KEYBOARD_MOVE_Y,
    COMMIT_MOUSE_SCALE,
    COMMIT_MOUSE_SCALE_UNIFORM,
    COMMIT_KEYBOARD_SCALE_UNIFORM,
    COMMIT_KEYBOARD_SCALE_X,
    COMMIT_KEYBOARD_SCALE_Y,
    COMMIT_MOUSE_ROTATE,
    COMMIT_KEYBOARD_ROTATE,
    COMMIT_MOUSE_SKEW_X,
    COMMIT_MOUSE_SKEW_Y,
    COMMIT_KEYBOARD_SKEW_X,
    COMMIT_KEYBOARD_SKEW_Y,
    COMMIT_FLIP_X,
    COMMIT_FLIP_Y
};

} // namespace UI
} // namespace Inkscape

#endif
