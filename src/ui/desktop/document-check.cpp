// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Check for data loss when closing a document window.
 *
 * Copyright (C) 2004-2021 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

/* Authors:
 *   MenTaLguY
 *   Link Mauve
 *   Thomas Holder
 *   Tavmjong Bah
 */

#include "document-check.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QWidget>

#include <glibmm/i18n.h>

#include "desktop.h"
#include "document.h"
#include "file.h"
#include "linea-window.h"
#include "object/sp-namedview.h"

/** Check if closing document associated with window will cause data loss, and if so opens a dialog
 *  that gives user options to save or ignore.
 *
 *  Returns true if document should remain open.
 */
bool document_check_for_data_loss(SPDesktop *desktop)
{
    g_assert(desktop);
    SPDocument *document = desktop->getDocument();
    LineaWindow *window = desktop->getLineaWindow();
    QWidget *parent = window ? static_cast<QWidget*>(window) : nullptr;

    if (document->isModifiedSinceSave()) {
        QString docName = QString::fromUtf8(document->getDocumentName());
        QString message = QString::fromUtf8(
            _("Save changes to document \"%1\" before closing?\n\n"
              "If you close without saving, your changes will be discarded."))
            .arg(docName);

        QMessageBox msgBox(parent);
        msgBox.setWindowTitle(QString::fromUtf8(_("Save Document")));
        msgBox.setText(message);
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.button(QMessageBox::Discard)->setText(QString::fromUtf8(_("Close without saving")));
        msgBox.button(QMessageBox::Save)->setText(QString::fromUtf8(_("Save")));

        int response = msgBox.exec();

        switch (response) {
            case QMessageBox::Save:
            {
                // Save document
                sp_namedview_document_from_window(desktop); // Save window geometry in document.
                // TODO: Need Qt version of sp_file_save_document
                // For now, returning true to prevent data loss until save is implemented
                return true;
            }
            case QMessageBox::Discard:
                break;
            default: // cancel pressed, or dialog was closed
                return true;
        }
    }

    // Check for data loss due to saving in lossy format.
    bool allow_data_loss = false;
    while (document->getReprRoot()->attribute("inkscape:dataloss") != nullptr && allow_data_loss == false) {
        QString docName = QString::fromUtf8(
            document->getDocumentName() ? document->getDocumentName() : _("Unnamed"));
        QString message = QString::fromUtf8(
            _("The file \"%1\" was saved with a format that may cause data loss!\n\n"
              "Do you want to save this file as Inkscape SVG?"))
            .arg(docName);

        QMessageBox msgBox(parent);
        msgBox.setWindowTitle(QString::fromUtf8(_("Save Document")));
        msgBox.setText(message);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.button(QMessageBox::Yes)->setText(QString::fromUtf8(_("Save as Inkscape SVG")));

        int response = msgBox.exec();

        switch (response) {
            case QMessageBox::Yes:
            {
                // TODO: Need Qt version of sp_file_save_dialog
                // For now, returning true to prevent data loss until save dialog is implemented
                return true;
            }
            case QMessageBox::No:
                allow_data_loss = true;
                break;
            default: // cancel pressed, or dialog was closed
                return true;
        }
    }

    return false;
}
