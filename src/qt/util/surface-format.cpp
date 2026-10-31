// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Window surface format helpers
 */

#include "surface-format.h"

#include <QColorSpace>
#include <QSurfaceFormat>
#include <glib.h>

#include "preferences.h"

namespace Linea::UI {

void applyPreferredColorSpace(QSurfaceFormat& format) {
    const auto cs = Inkscape::Preferences::get()->getString("/options/displayprofile/colorspace").lowercase().raw();
    if (cs.empty()) return;

    QColorSpace color_space;
    if (cs == "srgb") color_space = QColorSpace(QColorSpace::SRgb);
    else if (cs == "srgblinear") color_space = QColorSpace(QColorSpace::SRgbLinear);
    else if (cs == "adobergb") color_space = QColorSpace(QColorSpace::AdobeRgb);
    else if (cs == "displayp3" || cs == "p3") color_space = QColorSpace(QColorSpace::DisplayP3);
    else if (cs == "prophotorgb") color_space = QColorSpace(QColorSpace::ProPhotoRgb);
    else if (!cs.empty()) g_warning("Unknown /options/displayprofile/colorspace value: %s", cs.c_str());
    if (color_space.isValid()) {
        format.setColorSpace(color_space);
    }
}

} // namespace Linea::UI
