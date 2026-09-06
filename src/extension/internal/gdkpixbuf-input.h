// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_EXTENSION_INTERNAL_GDKPIXBUF_INPUT_H
#define INKSCAPE_EXTENSION_INTERNAL_GDKPIXBUF_INPUT_H

#include "extension/implementation/implementation.h"

namespace Inkscape::Extension::Internal {

class GdkpixbufInput : public Inkscape::Extension::Implementation::Implementation
{
public:
    std::unique_ptr<SPDocument> open(Inkscape::Extension::Input *mod, char const *uri, bool is_importing) override;
    static void init();
};

}  // namespace Inkscape::Extension::Internal

#endif /* INKSCAPE_EXTENSION_INTERNAL_GDKPIXBUF_INPUT_H */
