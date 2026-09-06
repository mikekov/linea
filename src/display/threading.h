// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: Liam White
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_DISPLAY_THREADING_H
#define INKSCAPE_DISPLAY_THREADING_H

#include <memory>

namespace Inkscape {

class dispatch_pool;

// Atomic accessor to global variable governing number of dispatch_pool threads.
void set_num_dispatch_threads(int num_dispatch_threads);

std::shared_ptr<dispatch_pool> get_global_dispatch_pool();

} // namespace Inkscape

#endif // INKSCAPE_DISPLAY_THREADING_H
