// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Render a preview strip of canvas control handles.
 */

#ifndef LINEA_UI_HANDLE_PREVIEW_H
#define LINEA_UI_HANDLE_PREVIEW_H

#include <QPixmap>

namespace Linea::UI {

/**
 * Render one sample of each handle type using the current
 * "/options/grabsize/value" setting and handle theme.
 * @param device_scale target devicePixelRatio (2.0 on high-res display)
 */
QPixmap draw_handles_preview(double device_scale);

} // namespace Linea::UI

#endif // LINEA_UI_HANDLE_PREVIEW_H
