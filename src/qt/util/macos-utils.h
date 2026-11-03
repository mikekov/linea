// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * macOS-specific application helpers
 */

#ifndef LINEA_QT_UTIL_MACOS_UTILS_H
#define LINEA_QT_UTIL_MACOS_UTILS_H

namespace Linea::UI {

/**
 * Bring the application to the foreground.
 *
 * Since macOS 26 a directly-launched process can stay inactive even after its
 * windows are shown; WindowServer then ignores cursor changes until the user
 * re-foregrounds the app (background + cmd-tab). Call after presenting a window
 * to force the activation through. No-op on other platforms.
 */
#ifdef __APPLE__
void activateApplication();
#else
inline void activateApplication() {}
#endif

} // namespace Linea::UI

#endif // LINEA_QT_UTIL_MACOS_UTILS_H
