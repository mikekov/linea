// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Traits::Reference - traits class for dealing with reference types
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2004 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_TRAITS_REFERENCE_H
#define SEEN_INKSCAPE_TRAITS_REFERENCE_H

namespace Inkscape {

namespace Traits {

template <typename T>
struct Reference {
    typedef T const &RValue;
    typedef T &LValue;
    typedef T *Pointer;
    typedef T const *ConstPointer;
};

template <typename T>
struct Reference<T &> {
    typedef T &RValue;
    typedef T &LValue;
    typedef T *Pointer;
    typedef T const *ConstPointer;
};

}

}

#endif
