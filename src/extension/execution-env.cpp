// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Abhishek Sharma
 *
 * Copyright (C) 2007-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "execution-env.h"

#include "desktop.h"
#include "document-undo.h"
#include "effect.h"
#include "inkscape.h"
#include "linea-window.h"
#include "selection.h"

#include <QMessageBox>
#include <QString>

#include "implementation/implementation.h"
#include "prefdialog/prefdialog.h"
#include "ui/widget/canvas.h" // To get window (perverse!)

namespace Inkscape {
namespace Extension {

/** \brief  Create an execution environment that will allow the effect
            to execute independently.
    \param effect  The effect that we should execute
    \param desktop     The Desktop with the document to work on
    \param docCache  The cache created for that document
    \param show_working  Show the working dialog
    \param show_error    Show the error dialog (not working)

    Grabs the selection of the current document so that it can get
    restored.  Will generate a document cache if one isn't provided.
*/
ExecutionEnv::ExecutionEnv (Effect * effect, SPDesktop *desktop, Implementation::ImplementationDocumentCache * docCache, bool show_working, bool show_errors) :
    _state(ExecutionEnv::INIT),
    _desktop(desktop),
    _docCache(docCache),
    _effect(effect),
    _show_working(show_working)
{
    if (_desktop) {
        document = desktop->doc();
    }
    if (document) {
        // Temporarily prevent undo in this scope
        Inkscape::DocumentUndo::ScopedInsensitive pauseUndo(document);
        Inkscape::Selection *selection = desktop->getSelection();
        if (selection) {
            // Make sure all selected objects have an ID attribute
            selection->enforceIds();
        }
        genDocCache();
    }
}

/** \brief  Destroy an execution environment

    Destroys the dialog if created and the document cache.
*/
ExecutionEnv::~ExecutionEnv () {
    if (_visibleDialog != nullptr) {
        _visibleDialog->hide();
        delete _visibleDialog;
        _visibleDialog = nullptr;
    }
    killDocCache();
    return;
}

/** \brief  Generate a document cache if needed

    If there isn't one we create a new one from the implementation
    from the effect's implementation.
*/
void
ExecutionEnv::genDocCache () {
    if (_docCache == nullptr && _desktop) {
        // printf("Gen Doc Cache\n");
        _docCache = _effect->get_imp()->newDocCache(_effect, _desktop);
    }
    return;
}

/** \brief  Destroy a document cache

    Just delete it.
*/
void
ExecutionEnv::killDocCache () {
    if (_docCache != nullptr) {
        // printf("Killed Doc Cache\n");
        delete _docCache;
        _docCache = nullptr;
    }
    return;
}

/** \brief  Create the working dialog

    Builds the dialog with a message saying that the effect is working.
    And make sure to connect to the cancel.
*/
void
ExecutionEnv::createWorkingDialog () {
    if (!_desktop)
        return;

    if (_visibleDialog != nullptr) {
        _visibleDialog->hide();
        delete _visibleDialog;
        _visibleDialog = nullptr;
    }

    auto const msg = QString::fromUtf8(_("'%1' complete, loading result...")).arg(QString::fromUtf8(_effect->get_name()));
    auto box = new QMessageBox(_popup_parent());
    box->setIcon(QMessageBox::Information);
    box->setText(msg);
    box->setStandardButtons(QMessageBox::Cancel);
    box->setModal(true);
    QObject::connect(box, &QMessageBox::rejected, [this]() { workingCanceled(0); });
    _visibleDialog = box;
    box->show();
}

QWidget* ExecutionEnv::_popup_parent() const {
    //QT TODO: get main window from desktop
    if (auto* dlg = _effect->get_pref_dialog())
        return dlg->window();
    return nullptr;
}

void
ExecutionEnv::workingCanceled( const int /*resp*/) {
    cancel();
    undo();
    return;
}

void
ExecutionEnv::cancel () {
    _desktop->clearWaitingCursor();
    _effect->get_imp()->cancelProcessing();
    return;
}

void
ExecutionEnv::undo () {
    DocumentUndo::cancel(document);
    return;
}

void
ExecutionEnv::commit () {
    DocumentUndo::done(document, Inkscape::Util::Internal::ContextString(_effect->get_name()), "");
    Effect::set_last_effect(_effect);
    _effect->get_imp()->commitDocument();
    killDocCache();
    return;
}

void
ExecutionEnv::reselect () {
    if (_desktop && _selectionState) {
        if (auto selection = _desktop->getSelection()) {
            selection->setState(*_selectionState);
        }
    }
    return;
}

void
ExecutionEnv::run () {
    _state = ExecutionEnv::RUNNING;

    if (_desktop) {
        if (_show_working) {
            createWorkingDialog();
        }
        auto selection = _desktop->getSelection();
        // Save selection state
        _selectionState = std::make_unique<Inkscape::SelectionState>(selection->getState());
        if (_show_working) {
            _desktop->setWaitingCursor();
        }
        _effect->get_imp()->effect(_effect, this, _desktop, _docCache);
        if (_show_working) {
            _desktop->clearWaitingCursor();
        }
        // Restore selection state
        selection->setState(*_selectionState);
        _selectionState.reset();
    } else {
        _effect->get_imp()->effect(_effect, this, document);
    }
    _state = ExecutionEnv::COMPLETE;
    // _runComplete.signal();
}

void
ExecutionEnv::runComplete () {
    _mainloop->quit();
}

bool
ExecutionEnv::wait () {
    if (_state != ExecutionEnv::COMPLETE) {
        if (_mainloop) {
            _mainloop = Glib::MainLoop::create(false);
        }

        sigc::connection conn = _runComplete.connect(sigc::mem_fun(*this, &ExecutionEnv::runComplete));
        _mainloop->run();

        conn.disconnect();
    }

    return true;
}



} }  /* namespace Inkscape, Extension */
