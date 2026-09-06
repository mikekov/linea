// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 * David Yip <yipdw@alumni.rose-hulman.edu>
 *   Abhishek Sharma
 *
 * Copyright (c) 2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_CONSOLE_OUTPUT_UNDO_OBSERVER_H
#define SEEN_INKSCAPE_CONSOLE_OUTPUT_UNDO_OBSERVER_H

#include <memory>

namespace Inkscape {
class UndoStackObserver;

/**
 * Create a Console Output Undo observer - an observer for tracing calls to
 * SPDocumentUndo::undo, SPDocumentUndo::redo, SPDocumentUndo::maybe_done.
 */
std::unique_ptr<UndoStackObserver> create_console_output_observer();
}

#endif // SEEN_INKSCAPE_CONSOLE_OUTPUT_UNDO_OBSERVER_H
