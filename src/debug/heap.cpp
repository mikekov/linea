// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Debug::Heap - interface for gathering heap statistics
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2005 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "inkgc/gc-alloc.h"
#include "debug/gc-heap.h"
#include "debug/sysv-heap.h"
#include <vector>

namespace Inkscape {
namespace Debug {

namespace {

typedef std::vector<Heap *, GC::Alloc<Heap *, GC::SCANNED, GC::MANUAL> > HeapCollection;

HeapCollection &heaps() {
    static bool is_initialized=false;
    static HeapCollection heaps;
    if (!is_initialized) {
        heaps.push_back(new SysVHeap());
        heaps.push_back(new GCHeap());
        is_initialized = true;
    }
    return heaps;
}

}

unsigned heap_count() {
    return heaps().size();
}

Heap *get_heap(unsigned i) {
    return heaps()[i];
}

void register_extra_heap(Heap &heap) {
    heaps().push_back(&heap);
}

}
}
