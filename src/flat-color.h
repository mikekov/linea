// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * FlatColor — A Color, Swatch, or None.
 */

#ifndef LINEA_FLAT_COLOR_H
#define LINEA_FLAT_COLOR_H

#include "colors/color.h"
#include <variant>
#include "swatch-type.h"

namespace Linea {

using FlatColor = std::variant<Inkscape::Colors::Color, Linea::Swatch, std::monostate>;

} // namespace Linea

#endif // LINEA_FLAT_COLOR_H
