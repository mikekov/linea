// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shared modeless-dialog helper for panel widgets.
 */

#ifndef LINEA_UI_DIALOG_UTILS_H
#define LINEA_UI_DIALOG_UTILS_H

#include <QSize>
#include <QString>
#include <functional>

class LineaWindow;
class QDialog;
class QWidget;
class SPDesktop;

namespace Linea::UI {

struct SharedDialogParams {
    /// Geometry-persistence path; also identifies the shared dialog instance.
    const char* prefsPath = nullptr;
    QString title;
    QSize defaultSize;
    /// Builds the content widget; it will be reparented into the dialog.
    std::function<QWidget* (QDialog*)> create;
    /// Optional: retarget the content to the current desktop — called at
    /// creation, on re-raise, and whenever the active desktop changes.
    std::function<void (QWidget*, SPDesktop*)> setDesktop;
};

/**
 * Show the shared modeless dialog identified by params.prefsPath, creating it
 * on first use and parenting it to `win`. Re-invocation raises the existing
 * dialog instead of creating a second one. Geometry is persisted via
 * persistGeometry. Returns the dialog, or nullptr when `win` has no desktop.
 */
QDialog* openSharedDialog(LineaWindow* win, const SharedDialogParams& params);

} // namespace Linea::UI

#endif // LINEA_UI_DIALOG_UTILS_H
