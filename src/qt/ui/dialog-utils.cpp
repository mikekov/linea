// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shared modeless-dialog helper for panel widgets.
 */

#include "dialog-utils.h"

#include <QDialog>
#include <QPointer>
#include <QVBoxLayout>
#include <unordered_map>

#include "linea-application.h"
#include "linea-window.h"
#include "widget-utils.h"

namespace Linea::UI {

namespace {

struct DialogEntry {
    QPointer<QDialog> dialog;
    QPointer<QWidget> content;
};

std::unordered_map<std::string, DialogEntry>& dialogs() {
    static std::unordered_map<std::string, DialogEntry> map;
    return map;
}

} // namespace

QDialog* openSharedDialog(LineaWindow* win, const SharedDialogParams& params) {
    auto& entry = dialogs()[params.prefsPath];

    if (entry.dialog) {
        if (params.setDesktop && entry.content) {
            if (auto dt = win ? win->get_desktop() : nullptr) {
                params.setDesktop(entry.content, dt);
            }
        }
        entry.dialog->show();
        entry.dialog->raise();
        entry.dialog->activateWindow();
        return entry.dialog;
    }

    if (!win || !win->get_desktop()) {
        return nullptr;
    }

    // Qt::Tool makes the dialog a floating utility window that stays above its
    // parent window (plain QDialog windows reorder freely and get covered).
    auto dialog = new QDialog(win, Qt::Tool);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(params.title);

    auto layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto content = params.create(dialog);
    if (!content) {
        delete dialog;
        return nullptr;
    }
    layout->addWidget(content);

    persistGeometry(dialog, params.prefsPath, params.defaultSize);

    // Follow the active desktop across tab switches and window focus changes.
    if (params.setDesktop) {
        auto setDesktop = params.setDesktop;
        QObject::connect(&LINEA_APP, &LineaApplication::activeDesktopChanged, dialog,
                         [content, setDesktop](SPDesktop* dt) { setDesktop(content, dt); });
    }

    dialog->show();

    entry.dialog = dialog;
    entry.content = content;
    return dialog;
}

} // namespace Linea::UI
