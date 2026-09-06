// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Build PDF patterns and gradients.
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef EXTENSION_INTERNAL_PDFOUTPUT_BUILD_PATTERNS_H
#define EXTENSION_INTERNAL_PDFOUTPUT_BUILD_PATTERNS_H

#include "build-drawing.h"

namespace Inkscape::Extension::Internal::PdfBuilder {

class PatternContext : public DrawContext
{
public:
    PatternContext(Document &doc, Geom::Rect const &bbox);
};

} // namespace Inkscape::Extension::Internal::PdfBuilder

#endif /* !EXTENSION_INTERNAL_PDFOUTPUT_BUILD_PATTERNS_H */
