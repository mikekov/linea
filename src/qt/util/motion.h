// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Qt animation / motion preference utilities
 */

#ifndef LINEA_QT_UTIL_MOTION_H
#define LINEA_QT_UTIL_MOTION_H

#include <QGuiApplication>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace Linea::UI {

/**
 * Whether the user has requested reduced motion at the OS level.
 * Reports false on Qt < 6.12, where Qt::MotionPreference did not exist.
 */
inline bool prefersReducedMotion() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    static auto hints = new QAccessibilityHints(qApp);
    return hints->motionPreference() == Qt::MotionPreference::ReducedMotion;
#else
    return false;
#endif
}

/**
 * Effective duration for a UI animation: 0 when the user prefers reduced
 * motion, so the animation jumps straight to its end state.
 */
inline int effectiveAnimationDuration(int msecs) {
    return prefersReducedMotion() ? 0 : msecs;
}

} // namespace Linea::UI

#endif // LINEA_QT_UTIL_MOTION_H
