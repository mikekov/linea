// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-float.h"

#include <iomanip>

#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>

#include "extension/extension.h"
#include "number-edit.h"
#include "preferences.h"
#include "spin-scale.h"
#include "util-string/ustring-format.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ParamFloat::ParamFloat(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // get value
    if (xml->firstChild()) {
        const char *value = xml->firstChild()->content();
        if (value)
            string_to_value(value);
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getDouble(pref_name(), _value);

    // parse and apply limits
    const char *min = xml->attribute("min");
    if (min) {
        _min = g_ascii_strtod(min, nullptr);
    }

    const char *max = xml->attribute("max");
    if (max) {
        _max = g_ascii_strtod(max, nullptr);
    }

    if (_value < _min) {
        _value = _min;
    }

    if (_value > _max) {
        _value = _max;
    }

    // parse precision
    const char *precision = xml->attribute("precision");
    if (precision != nullptr) {
        _precision = strtol(precision, nullptr, 0);
    }


    // parse appearance
    if (_appearance) {
        if (!strcmp(_appearance, "full")) {
            _mode = FULL;
        } else {
            g_warning("Invalid value ('%s') for appearance of parameter '%s' in extension '%s'",
                      _appearance, _name, _extension->get_id());
        }
    }
}

/**
 * A function to set the \c _value.
 *
 * This function sets the internal value, but it also sets the value
 * in the preferences structure.  To put it in the right place \c pref_name() is used.
 *
 * @param  in   The value to set to.
 */
double ParamFloat::set(double in)
{
    _value = in;
    if (_value > _max) {
        _value = _max;
    }
    if (_value < _min) {
        _value = _min;
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble(pref_name(), _value);

    return _value;
}

std::string ParamFloat::value_to_string() const
{
    return Inkscape::ustring::format_classic(std::setprecision(_precision), std::fixed, _value);
}

void ParamFloat::string_to_value(const std::string &in)
{
    _value = g_ascii_strtod(in.c_str(), nullptr);
}

/**
 * Creates a Float Adjustment for a float parameter.
 *
 * Builds a hbox with a label and a float adjustment in it.
 */
QWidget* ParamFloat::get_widget(sigc::signal<void ()>* changeSignal)
{
    if (_hidden) {
        return nullptr;
    }

    auto const hbox = new QWidget();
    auto const layout = new QHBoxLayout(hbox);
    layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);

    auto const label = new QLabel(QString::fromUtf8(_text));
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(label, 1);

    if (_mode == FULL) {
        auto const scale = new Linea::UI::SpinScale();
        scale->setRange(_min, _max);
        scale->setDecimals(_precision);
        scale->numberEdit()->setSingleStep(0.1);
        scale->setValue(_value);
        layout->addWidget(scale, 0);

        QObject::connect(scale, &Linea::UI::SpinScale::valueChanged,
                         [this, changeSignal](double val) {
            set(val);
            if (changeSignal != nullptr) {
                changeSignal->emit();
            }
        });
    } else {
        auto const spin = new Linea::UI::NumberEdit();
        spin->setRange(_min, _max);
        spin->setDecimals(_precision);
        spin->setSingleStep(0.1);
        spin->setValue(_value);
        layout->addWidget(spin, 0);

        QObject::connect(spin, &Linea::UI::NumberEdit::valueChanged,
                         [this, changeSignal](double val) {
            set(val);
            if (changeSignal != nullptr) {
                changeSignal->emit();
            }
        });
    }

    return hbox;
}

} // namespace Inkscape::Extension
