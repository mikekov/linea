// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Mini static library that contains the version of Inkscape
 *
 * This is better than a header file, because it only requires a recompile
 * of a single file and a relink to update the version.
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2008 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_INKSCAPE_VERSION_H
#define SEEN_INKSCAPE_INKSCAPE_VERSION_H

namespace Inkscape {

extern char const *revision_string; ///< code revision string
extern char const *version_string; ///< full version string
extern char const *version_string_without_revision; ///< version string excluding revision and date

extern unsigned short int const build_year; ///< four digit year of build

extern unsigned int const version_major;
extern unsigned int const version_minor;
extern unsigned int const version_patch;

} // namespace Inkscape

namespace Linea {

extern const char* version_string_long;
extern const char* version_string_short;
extern unsigned int const version_major;
extern unsigned int const version_minor;
extern unsigned int const version_patch;

} // namespace Linea

#endif
