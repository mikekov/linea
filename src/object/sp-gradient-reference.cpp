// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "sp-gradient-reference.h"

bool SPGradientReference::_acceptObject(SPObject *obj) const
{
    return is<SPGradient>(obj) && URIReference::_acceptObject(obj);
    /* effic: Don't bother making this an inline function: _acceptObject is a virtual function,
       typically called from a context where the runtime type is not known at compile time. */
}
