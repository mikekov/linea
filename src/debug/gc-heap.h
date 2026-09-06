// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Debug::GCHeap - heap statistics for libgc heap
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2004 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_DEBUG_GC_HEAP_H
#define SEEN_INKSCAPE_DEBUG_GC_HEAP_H

#include "inkgc/gc-core.h"
#include "debug/heap.h"

namespace Inkscape {
namespace Debug {

class GCHeap : public Debug::Heap {
public:
    int features() const override {
        return SIZE_AVAILABLE | USED_AVAILABLE | GARBAGE_COLLECTED;
    }
    char const *name() const override {
        return "libgc";
    }
    Heap::Stats stats() const override {
        Stats stats;
        stats.size = GC::Core::get_heap_size();
        stats.bytes_used = stats.size - GC::Core::get_free_bytes();
        return stats;
    }
    void force_collect() override { GC::Core::gcollect(); }
};

}
}

#endif
