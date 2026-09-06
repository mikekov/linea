// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Conversion between BitmapView (ARGB32 premultiplied) and internal trace map types.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2013 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_TRACE_IMAGEMAP_CONVERT_H
#define INKSCAPE_TRACE_IMAGEMAP_CONVERT_H

#include <QImage>
#include <cstdint>
#include <vector>
#include "imagemap.h"
#include "trace/trace.h"

namespace Inkscape {
namespace Trace {

GrayMap bitmapToGrayMap(const BitmapView&);
RgbMap  bitmapToRgbMap(const BitmapView&);
QImage  grayMapToQImage(const GrayMap&);
QImage  indexedMapToQImage(const IndexedMap&);
QImage  bitmapToQImagePreview(const BitmapView&);

/// Convert un-premultiplied ARGB uint32_t values (0xAARRGGBB) to a premultiplied QImage.
QImage  argbToQImage(const uint32_t* argb, int width, int height);

/// Convert ARGB32 premultiplied to un-premultiplied ARGB uint32_t values.
/// Each pixel is packed as 0xAARRGGBB.
std::vector<uint32_t> bitmapToArgb(const BitmapView&);

/// Convert ARGB32 premultiplied to tightly packed RGB8 (composited over white, no alpha).
/// Suitable for libraries that expect packed RGB with no rowstride.
std::vector<unsigned char> bitmapToRgb8Packed(const BitmapView&);

/// Convert ARGB32 premultiplied to tightly packed RGBA (un-premultiplied).
/// Suitable for libraries that expect GdkPixbuf-style RGBA byte order.
std::vector<unsigned char> bitmapToRgba(const BitmapView&);

} // namespace Trace
} // namespace Inkscape

#endif // INKSCAPE_TRACE_IMAGEMAP_CONVERT_H
