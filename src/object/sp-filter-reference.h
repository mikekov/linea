// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_FILTER_REFERENCE_H
#define SEEN_SP_FILTER_REFERENCE_H

#include "uri-references.h"
#include "sp-filter.h" // Required for the static_cast.

class SPObject;
class SPDocument;

class SPFilterReference : public Inkscape::URIReference {
public:
    SPFilterReference(SPObject *obj) : URIReference(obj) {}
    SPFilterReference(SPDocument *doc) : URIReference(doc) {}

    SPFilter *getObject() const {
        return static_cast<SPFilter *>(URIReference::getObject());
    }

protected:
    bool _acceptObject(SPObject *obj) const override;
};

#endif /* !SEEN_SP_FILTER_REFERENCE_H */
