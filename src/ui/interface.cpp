// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main UI stuff.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2012 Kris De Gussem
 * Copyright (C) 2010 authors
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "interface.h"

#include <glibmm/convert.h>      // for filename_to_utf8
#include <glibmm/i18n.h>         // for _
#include <glibmm/miscutils.h>    // for path_get_basename, path_get_dirname
#include <QAbstractButton>
#include <QMessageBox>
#include <QWidget>

#include "desktop.h" // for SPDesktop
#include "linea-application.h"
#include "linea-window.h"
#include "ui/desktop/desktop-widget.h"
#include "io/sys.h"                // for file_test, sanitizeString
#include "inkscape.h" // for Application, SP_ACTIVE_DOCUMENT

class SPDocument;

Glib::ustring getLayoutPrefPath(SPDesktop *desktop)
{
    if (desktop->is_focusMode()) {
        return "/focus/";
    } else if (desktop->is_fullscreen()) {
        return "/fullscreen/";
    } else {
        return "/window/";
    }
}

void sp_ui_error_dialog(char const *message)
{
    auto const safeMsg = Inkscape::IO::sanitizeString(message);
    auto const error = QString::fromUtf8(safeMsg.c_str()).toHtmlEscaped();
    auto& app = LineaApplication::instance();
    if (auto window = app.get_active_window()) {
        window->getDesktopWidget()->showError(QString::fromUtf8(_("Error")), error);
        return;
    }

    QWidget *parent = nullptr;
    if (auto desktop = SP_ACTIVE_DESKTOP) {
        parent = desktop->getLineaWindow();
    }
    QMessageBox::critical(parent, QString::fromUtf8(_("Error")),
        QString::fromUtf8(safeMsg.c_str()));
}

/**
 * If necessary, ask the user if a file may be overwritten.
 *
 * @arg filename path to file.
 * Value is in platform-native encoding (see Glib::filename_to_utf8).
 * @returns true if it is okay to write to the file.
 * This means that the file does not exist yet or the user confirmed that overwriting is okay.
 */
bool sp_ui_overwrite_file(std::string const &filename)
{
    if (!g_file_test(filename.c_str(), G_FILE_TEST_EXISTS)) {
        return true;
    }

    auto const basename = Glib::filename_to_utf8(Glib::path_get_basename(filename));
    auto const dirname = Glib::filename_to_utf8(Glib::path_get_dirname(filename));
    auto const msg = QString::fromUtf8(
        _("A file named \"%1\" already exists. Do you want to replace it?\n\n"
          "The file already exists in \"%2\". Replacing it will overwrite its contents."))
        .arg(QString::fromUtf8(basename.c_str()))
        .arg(QString::fromUtf8(dirname.c_str()));

    QWidget *parent = nullptr;
    if (auto desktop = SP_ACTIVE_DESKTOP) {
        parent = desktop->getLineaWindow();
    }

    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(QString::fromUtf8(_("Replace File")));
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.button(QMessageBox::Yes)->setText(QString::fromUtf8(_("Replace")));
    msgBox.button(QMessageBox::No)->setText(QString::fromUtf8(_("Cancel")));

    return msgBox.exec() == QMessageBox::Yes;
}
