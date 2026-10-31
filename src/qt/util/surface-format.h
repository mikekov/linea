// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Window surface format helpers
 */

#ifndef LINEA_QT_UTIL_SURFACE_FORMAT_H
#define LINEA_QT_UTIL_SURFACE_FORMAT_H

class QSurfaceFormat;

namespace Linea::UI {

/**
 * Apply the color space named in /options/displayprofile/colorspace to the
 * given surface format. Unset or unknown values leave the format untouched.
 * Must be called before any QWindow is constructed — the default format is
 * captured in the QWindow constructor.
 */
void applyPreferredColorSpace(QSurfaceFormat& format);

/**
 * True when /options/displayprofile/colorspace selects a Display P3 surface —
 * i.e. raw bytes written to the window surface are interpreted as P3, so
 * sRGB-encoded content needs converting and wide-gamut content may be emitted
 * directly.
 */
bool displaySurfaceIsP3();

} // namespace Linea::UI

#endif // LINEA_QT_UTIL_SURFACE_FORMAT_H
