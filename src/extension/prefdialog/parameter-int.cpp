// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-int.h"

#include <cstring>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>

#include "extension/extension.h"
#include "number-edit.h"
#include "preferences.h"
#include "spin-scale.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ParamInt::ParamInt(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // get value
    if (xml->firstChild()) {
        const char *value = xml->firstChild()->content();
        if (value)
            string_to_value(value);
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getInt(pref_name(), _value);

    // parse and apply limits
    const char *min = xml->attribute("min");
    if (min) {
        _min = std::strtol(min, nullptr, 0);
    }

    const char *max = xml->attribute("max");
    if (max) {
        _max = std::strtol(max, nullptr, 0);
    }

    if (_value < _min) {
        _value = _min;
    }

    if (_value > _max) {
        _value = _max;
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
 * This function sets the internal value, but it also sets the value
 * in the preferences structure.  To put it in the right place \c pref_name() is used.
 *
 * @param  in   The value to set to.
 */
int ParamInt::set(int in)
{
    _value = in;
    if (_value > _max) {
        _value = _max;
    }
    if (_value < _min) {
        _value = _min;
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setInt(pref_name(), _value);

    return _value;
}

/**
 * Creates a Int Adjustment for a int parameter.
 *
 * Builds a hbox with a label and a int adjustment in it.
 */
QWidget* ParamInt::get_widget(sigc::signal<void ()>* changeSignal) {
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
        scale->setDecimals(0);
        scale->numberEdit()->setSingleStep(1.0);
        scale->setValue(_value);
        layout->addWidget(scale, 0);

        QObject::connect(scale, &Linea::UI::SpinScale::valueChanged,
                         [this, changeSignal](double val) {
            set(static_cast<int>(val));
            if (changeSignal != nullptr) {
                changeSignal->emit();
            }
        });
    } else {
        auto const spin = new Linea::UI::NumberEdit();
        spin->setRange(_min, _max);
        spin->setDecimals(0);
        spin->setSingleStep(1.0);
        spin->setValue(_value);
        layout->addWidget(spin, 0);

        QObject::connect(spin, &Linea::UI::NumberEdit::valueChanged,
                         [this, changeSignal](double val) {
            set(static_cast<int>(val));
            if (changeSignal != nullptr) {
                changeSignal->emit();
            }
        });
    }

    return hbox;
}

std::string ParamInt::value_to_string() const
{
    char value_string[32];
    std::snprintf(value_string, 32, "%d", _value);
    return value_string;
}

void ParamInt::string_to_value(const std::string &in)
{
    _value = std::strtol(in.c_str(), nullptr, 0);
}

} // namespace Inkscape::Extension
