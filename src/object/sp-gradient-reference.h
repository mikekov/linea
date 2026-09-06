// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_GRADIENT_REFERENCE_H
#define SEEN_SP_GRADIENT_REFERENCE_H

#include "uri-references.h"
#include "sp-gradient.h"

class SPGradientReference : public Inkscape::URIReference
{
public:
    SPGradientReference(SPGradient *grad) : URIReference(grad) {}

    SPGradient *getObject() const {
        return static_cast<SPGradient *>(URIReference::getObject());
    }

protected:
    bool _acceptObject(SPObject *obj) const override;
};

#endif /* !SEEN_SP_GRADIENT_REFERENCE_H */
