// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *
 * Copyright (C) 2005-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "prefdialog.h"

#include <cassert>
#include <glibmm/i18n.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QFrame>
#include <QEventLoop>
#include <QTimer>
#include <QPushButton>

#include "document.h"
#include "document-undo.h"
#include "extension/effect.h"
#include "extension/execution-env.h"
#include "extension/implementation/implementation.h"
#include "inkscape.h" // Used to get SP_ACTIVE_DESKTOP
#include "parameter.h"
#include "xml/repr.h"

namespace Inkscape::Extension {

/** \brief  Creates a new preference dialog for extension preferences
    \param  name      Name of the Extension whose dialog this is (should already be translated)
    \param  controls  The extension specific widgets in the dialog

    This function initializes the dialog with the name of the extension
    in the title.  It adds a few buttons and sets up handlers for
    them.  It also places the passed-in widgets into the dialog.
*/
PrefDialog::PrefDialog(Glib::ustring name, QWidget* controls, Effect* effect) :
    _name(name),
    _button_ok(nullptr),
    _button_cancel(nullptr),
    _button_preview(nullptr),
    _checkbox_preview(nullptr),
    _effect(effect)
{
    _popup = new QDialog(nullptr); // no parent for now; callers can re-parent via window()
    _popup->setWindowTitle(QString::fromStdString(_name.raw()));
    auto const main_layout = new QVBoxLayout(_popup);
    main_layout->setContentsMargins(InxWidget::GUI_BOX_MARGIN, InxWidget::GUI_BOX_MARGIN,
                                    InxWidget::GUI_BOX_MARGIN, InxWidget::GUI_BOX_MARGIN);
    main_layout->setSpacing(InxWidget::GUI_BOX_SPACING);

    if (controls == nullptr) {
        if (_effect == nullptr) {
            std::cerr << "AH!!!  No controls and no effect!!!" << std::endl;
            return;
        }
        controls = _effect->get_imp()->prefs_effect(_effect, SP_ACTIVE_DESKTOP, &_signal_param_change, nullptr);
        _signal_param_change.connect(sigc::mem_fun(*this, &PrefDialog::param_change));
    }

    if (controls) {
        main_layout->addWidget(controls, 1);
    }

    // Buttons — use standard buttons for proper sizing and uniform width
    auto const button_box = new QDialogButtonBox(
        _effect == nullptr ? (QDialogButtonBox::Ok | QDialogButtonBox::Cancel)
                           : (QDialogButtonBox::Apply | QDialogButtonBox::Close),
        Qt::Horizontal, _popup);
    _button_ok = button_box->button(
        _effect == nullptr ? QDialogButtonBox::Ok : QDialogButtonBox::Apply);
    _button_cancel = button_box->button(
        _effect == nullptr ? QDialogButtonBox::Cancel : QDialogButtonBox::Close);
    _button_ok->setDefault(true);
    _button_ok->setFocus();
    _button_ok->setMinimumWidth(80);
    _button_cancel->setMinimumWidth(80);

    main_layout->addWidget(button_box);

    // Apply has ApplyRole, which does not trigger QDialogButtonBox::accepted
    // (only AcceptRole/YesRole do). Connect it directly so Apply actually
    // commits the preview instead of being a no-op.
    QObject::connect(_button_ok, &QPushButton::clicked, _popup, [this]() { on_response(1 /* OK */); });
    // Route both the Close button and the window X button through the dialog's
    // reject() so they share a single path. QDialogButtonBox::rejected (Close
    // click) and QDialog::rejected (X close) are different signals on different
    // objects; connecting the button box to the dialog's reject() unifies them.
    QObject::connect(button_box, &QDialogButtonBox::rejected, _popup, [this]() { _popup->reject(); });
    QObject::connect(_popup, &QDialog::rejected, [this]() { on_response(0 /* CANCEL */); });

    if (_effect != nullptr && !_effect->no_live_preview) {
        if (_param_preview == nullptr) {
            XML::Document* doc = sp_repr_read_mem(live_param_xml, strlen(live_param_xml), nullptr);
            if (doc == nullptr) {
                std::cerr << "Error encountered loading live parameter XML !!!" << std::endl;
                return;
            }

            _param_preview.reset(InxParameter::make(doc->root(), _effect));
        }

        auto const sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        main_layout->addWidget(sep);

        _button_preview = _param_preview->get_widget(&_signal_preview);
        if (_button_preview) {
            main_layout->addWidget(_button_preview);
        }

        // The preview widget is a QCheckBox inside a layout (from ParamBool::get_widget).
        // Find the checkbox so we can query its state.
        const auto checkboxes = _button_preview ? _button_preview->findChildren<QCheckBox*>() : QList<QCheckBox*>();
        if (!checkboxes.isEmpty()) {
            _checkbox_preview = checkboxes.first();
        }

        // ParamBool reads its value from preferences, which may have a stale
        // "false" from a previous session. Force the checkbox on so the param
        // value and checkbox state match the XML default (true).
        if (_checkbox_preview && !_checkbox_preview->isChecked()) {
            _checkbox_preview->setChecked(true);
        }

        preview_toggle();
        _signal_preview.connect(sigc::mem_fun(*this, &PrefDialog::preview_toggle));
    }

    // Set window modality for effects that don't use live preview
    //QT TODO
    // if (_effect != nullptr && _effect->no_live_preview) {
        // set_modal(false);
    // }
}

PrefDialog::~PrefDialog ( )
{
    if (_timer) {
        _timer->stop();
        // _timer is parented to _popup, deleted below.
    }

    if (_exEnv != nullptr) {
        _exEnv->cancel();
    }

    if (_effect != nullptr) {
        _effect->set_pref_dialog(nullptr);
    }

    delete _popup;
    return;
}

void
PrefDialog::preview_toggle () {
    // This wrap prevent crashes on fast click
    // We are on 2 mains process so can triger a crash if you check too fast
    if (_button_preview) _button_preview->setEnabled(false);
    SPDocument* document = SP_ACTIVE_DOCUMENT;
    bool modified = document->isModifiedSinceSave();

    assert(_param_preview);
    if(_param_preview->get_bool()) {
        if (_exEnv == nullptr) {
            _exEnv = std::make_unique<ExecutionEnv>(_effect, SP_ACTIVE_DESKTOP, nullptr, false, false);
            _exEnv->run();
        }
    } else {
        if (_exEnv != nullptr) {
            _exEnv->cancel();
            _exEnv->undo();
            _exEnv->reselect();

            _exEnv.reset();
        }
    }

    document->setModifiedSinceSave(modified);
    if (_button_preview) _button_preview->setEnabled(true);
}

void
PrefDialog::param_change () {
    if (_exEnv != nullptr) {
        if (!_effect->loaded()) {
            _effect->set_state(Extension::STATE_LOADED);
        }
        if (!_timer) {
            _timer = new QTimer(_popup);
            _timer->setSingleShot(true);
            QObject::connect(_timer, &QTimer::timeout, [this]() { param_timer_expire(); });
        }
        _timer->start(250);
    }
}

bool
PrefDialog::param_timer_expire () {
    if (_exEnv != nullptr) {
        _exEnv->cancel();
        _exEnv->undo();
        _exEnv->reselect();
        _exEnv->run();
    }

    return false;
}

void
PrefDialog::on_response (int signal) {
    // Stop any pending param-change timer so it doesn't fire after we've
    // committed/undone the preview or after the dialog is deleted.
    if (_timer) {
        _timer->stop();
    }

    if (signal == 1 /* OK */) {
        if (_exEnv == nullptr) {
            if (_effect != nullptr) {
                _effect->effect(SP_ACTIVE_DESKTOP);
            } else {
                // Shutdown run()
                if (_popup) _popup->done(1);
                return;
            }
        } else {
            if (_exEnv->wait()) {
                _exEnv->commit();
            } else {
                _exEnv->undo();
                _exEnv->reselect();
            }

            _exEnv.reset();
        }
    }

    // Tear down live preview explicitly. Don't call setChecked(false) — it
    // triggers preview_toggle() re-entrantly while we're tearing down.
    if (_exEnv != nullptr) {
        _exEnv->cancel();
        if (signal == 0 /* CANCEL */) {
            _exEnv->undo();
            _exEnv->reselect();
        }
        _exEnv.reset();
    }

    if (signal == 0 /* CANCEL */) {
        // reject() (which routed us here) already calls done(0); calling it
        // again would re-enter reject() -> rejected -> on_response recursively.
        if (_effect != nullptr) {
            // Non-modal: defer deletion to avoid use-after-free
            // while still inside the button box signal handler.
            QTimer::singleShot(0, [this]() { delete this; });
        }
    } else if (signal == 1 /* OK */ && _effect == nullptr) {
        // Non-effect path: close popup so run() returns
        if (_popup) _popup->done(1);
    }
}

void PrefDialog::show() {
    if (_popup) {
        _popup->show();
        _popup->raise();
        _popup->activateWindow();
    }
}

void PrefDialog::present() {
    show();
}

QWidget* PrefDialog::window() {
    return _popup;
}

int PrefDialog::run() {
    if (!_popup) return 0;
    return _popup->exec();
}

#include "extension/internal/clear-n_.h"

const char* PrefDialog::live_param_xml =
    R"(<param name="__live_effect__" type="bool" gui-text=")" N_("Live preview")
    R"(" gui-description=")" N_("Is the effect previewed live on canvas?") R"(">true</param>)";

} // namespace Inkscape::Extension
