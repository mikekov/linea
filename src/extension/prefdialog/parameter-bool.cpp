// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-bool.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QWidget>

#include "extension/extension.h"
#include "preferences.h"
#include "xml/node.h"

namespace Inkscape {
namespace Extension {

ParamBool::ParamBool(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // get value
    if (xml->firstChild()) {
        const char *value = xml->firstChild()->content();
        if (value)
            string_to_value(value);
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getBool(pref_name(), _value);
}

bool ParamBool::get() const
{
    return _value;
}

bool ParamBool::set(bool in)
{
    _value = in;

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setBool(pref_name(), _value);

    return _value;
}

/**
 * A check button which is Param aware.  It works with the
 * parameter to change it's value as the check button changes
 * value.
 */
class ParamBoolCheckButton : public QCheckBox {
public:
    /**
     * Initialize the check button.
     * This function sets the value of the checkbox to be that of the
     * parameter, and then sets up a callback to \c on_toggle.
     *
     * @param  param  Which parameter to adjust on changing the check button
     */
    ParamBoolCheckButton(ParamBool* param, char* label, sigc::signal<void ()>* changeSignal)
        : QCheckBox(QString::fromUtf8(label))
        , _pref(param)
        , _changeSignal(changeSignal) {
        this->setChecked(_pref->get());
        QObject::connect(this, &QCheckBox::toggled, this, &ParamBoolCheckButton::on_toggle);
    }

    /**
     * A function to respond to the check box changing.
     * Adjusts the value of the preference to match that in the check box.
     */
    void on_toggle(bool checked);

private:
    /** Param to change. */
    ParamBool* _pref;
    sigc::signal<void ()>* _changeSignal;
};

void ParamBoolCheckButton::on_toggle(bool checked)
{
    _pref->set(checked);
    if (_changeSignal != nullptr) {
        _changeSignal->emit();
    }
}

std::string ParamBool::value_to_string() const
{
    return _value ? "true" : "false";
}

void ParamBool::string_to_value(const std::string &in)
{
    if (in == "true") {
        _value = true;
    } else if (in == "false") {
        _value = false;
    } else {
        g_warning("Invalid default value ('%s') for parameter '%s' in extension '%s'", in.c_str(), _name,
                  _extension->get_id());
    }
}

QWidget* ParamBool::get_widget(sigc::signal<void ()>* changeSignal) {
    if (_hidden) {
        return nullptr;
    }

    auto const hbox = new QWidget();
    auto const layout = new QHBoxLayout(hbox);
    layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);

    auto const checkbox = new ParamBoolCheckButton(this, _text, changeSignal);
    layout->addWidget(checkbox);

    return hbox;
}

}  /* namespace Extension */
}  /* namespace Inkscape */
