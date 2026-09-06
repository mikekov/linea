// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Conversion utilities between Qt QFont and Pango font spec strings.
 *
 * The codebase uses Pango font descriptions internally (e.g. LPEs parse
 * font values with Pango::FontDescription), but Qt UI widgets produce
 * QFont objects. These functions bridge the two worlds.
 */

#ifndef LIBNRTYPE_FONT_QT_BRIDGE_H
#define LIBNRTYPE_FONT_QT_BRIDGE_H

#include <glibmm/ustring.h>

class QFont;

namespace Linea {

/// Convert a QFont to a Pango-compatible font spec string (e.g. "Sans Serif 12pt").
/// Used when storing font choices from Qt widgets (QFontComboBox) for LPE consumption.
Glib::ustring qfont_to_pango_string(const QFont& font);

/// Convert a Pango font spec string to a QFont.
/// Used when loading saved font values into Qt widgets.
QFont pango_string_to_qfont(const Glib::ustring& spec);

} // namespace Linea

#endif // LIBNRTYPE_FONT_QT_BRIDGE_H
