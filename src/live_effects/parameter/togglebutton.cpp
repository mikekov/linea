// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Jabiertxo Arraiza Cenoz 2014 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "togglebutton.h"

#include <utility>
#include <glibmm/i18n.h>

#include <QPushButton>
#include <QIcon>
#include <QString>

#include "inkscape.h"
#include "selection.h"

#include "live_effects/effect.h"
#include "util/numeric/converters.h"

namespace Inkscape::LivePathEffect {

ToggleButtonParam::ToggleButtonParam(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                                     Inkscape::UI::Widget::Registry *wr, Effect *effect, bool default_value,
                                     Glib::ustring inactive_label, char const *_icon_active, char const *_icon_inactive)
    : Parameter(label, tip, key, wr, effect)
    , value(default_value)
    , defvalue(default_value)
    , inactive_label(std::move(inactive_label))
    , _icon_active(_icon_active)
    , _icon_inactive(_icon_inactive)
{
    checkwdg = nullptr;
}

ToggleButtonParam::~ToggleButtonParam() {
    if (_toggled_connection) {
        QObject::disconnect(_toggled_connection);
    }
}

void
ToggleButtonParam::param_set_default()
{
    param_setValue(defvalue);
}

bool
ToggleButtonParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(Inkscape::Util::read_bool(strvalue, defvalue));
    return true; // not correct: if value is unacceptable, should return false!
}

Glib::ustring
ToggleButtonParam::param_getSVGValue() const
{
    return value ? "true" : "false";
}

Glib::ustring
ToggleButtonParam::param_getDefaultSVGValue() const
{
    return defvalue ? "true" : "false";
}

void 
ToggleButtonParam::param_update_default(bool default_value)
{
    defvalue = default_value;
}

void 
ToggleButtonParam::param_update_default(const gchar * default_value)
{
    param_update_default(Inkscape::Util::read_bool(default_value, defvalue));
}

QWidget*
ToggleButtonParam::param_newWidget()
{
    if (_toggled_connection) {
        QObject::disconnect(_toggled_connection);
    }

    auto* btn = new QPushButton();
    checkwdg = btn;
    update_button_label();

    btn->setCheckable(true);
    btn->setChecked(value);

    _toggled_connection = QObject::connect(btn, &QPushButton::toggled, [this]() { toggled(); });
    return btn;
}

void
ToggleButtonParam::refresh_button()
{
    if (!checkwdg) return;
    update_button_label();
    checkwdg->setChecked(value);
}

void
ToggleButtonParam::update_button_label()
{
    if (!checkwdg) return;

    QString text;
    if (value || inactive_label.empty()) {
        text = QString::fromStdString(param_label.raw());
    } else {
        text = QString::fromStdString(inactive_label.raw());
    }

    if (_icon_active) {
        if (!_icon_inactive) {
            _icon_inactive = _icon_active;
        }
        checkwdg->setIcon(QIcon::fromTheme(QString::fromUtf8(value ? _icon_active : _icon_inactive)));
    }
    checkwdg->setText(text);
}

void
ToggleButtonParam::param_setValue(bool newvalue)
{
    if (value != newvalue) {
        param_effect->refresh_widgets = true;
    }
    value = newvalue;
    refresh_button();
}

void
ToggleButtonParam::toggled() {
    if (SP_ACTIVE_DESKTOP) {
        Inkscape::Selection *selection = SP_ACTIVE_DESKTOP->getSelection();
        selection->emitModified();
    }
    _signal_toggled.emit();
}

} // namespace Inkscape::LivePathEffect
