// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Conversion utilities between Qt QFont and Pango font spec strings.
 */

#include "font-qt-bridge.h"

#include <QFont>
#include <QString>
#include <pangomm/font.h>
#include <pangomm/fontdescription.h>

namespace Linea {

Glib::ustring qfont_to_pango_string(const QFont& font) {
    int pt = font.pointSize();
    if (pt <= 0) {
        pt = 12;
    }
    return Glib::ustring::compose("%1 %2", font.family().toStdString(), pt);
}

QFont pango_string_to_qfont(const Glib::ustring& spec) {
    // Try Qt's own format first (for files saved by older Qt versions).
    QFont font;
    if (font.fromString(QString::fromStdString(spec.raw()))) {
        return font;
    }

    // Otherwise parse as Pango font description.
    Pango::FontDescription desc(spec);
    auto family = desc.get_family();
    if (!family.empty()) {
        font.setFamily(QString::fromStdString(family));
    }
    double size = desc.get_size() / static_cast<double>(Pango::SCALE);
    if (size > 0) {
        font.setPointSizeF(size);
    }
    return font;
}

} // namespace Linea
