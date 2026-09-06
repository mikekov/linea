// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * SVG attribute formatting helpers for the Qt filter editor.
 */

#ifndef LINEA_UI_FILTER_SVG_ATTRIBUTE_FORMAT_H
#define LINEA_UI_FILTER_SVG_ATTRIBUTE_FORMAT_H

#include <QString>

#include "svg/css-ostringstream.h"

namespace Linea::UI::Filter {

inline QString format_number(double value) {
    Inkscape::CSSOStringStream stream;
    stream << value;
    return QString::fromStdString(stream.str());
}

} // namespace Linea::UI::Filter

#endif // LINEA_UI_FILTER_SVG_ATTRIBUTE_FORMAT_H
