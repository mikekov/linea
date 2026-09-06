// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "bool.h"

#include <glibmm/i18n.h>

#include <QCheckBox>
#include <QHBoxLayout>

#include "live_effects/effect.h"
#include "util/numeric/converters.h"


namespace Inkscape {

namespace LivePathEffect {

BoolParam::BoolParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, bool default_value)
    : Parameter(label, tip, key, wr, effect), value(default_value), defvalue(default_value)
{
}

BoolParam::~BoolParam() = default;

void
BoolParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
BoolParam::param_update_default(bool const default_value)
{
    defvalue = default_value;
}

void 
BoolParam::param_update_default(const gchar * default_value)
{
    param_update_default(Inkscape::Util::read_bool(default_value, defvalue));
}

bool
BoolParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(Inkscape::Util::read_bool(strvalue, defvalue));
    return true; // not correct: if value is unacceptable, should return false!
}

Glib::ustring
BoolParam::param_getSVGValue() const
{
    return value ? "true" : "false";
}

Glib::ustring
BoolParam::param_getDefaultSVGValue() const
{
    return defvalue ? "true" : "false";
}

QWidget*
BoolParam::param_newWidget()
{
    if (!widget_is_visible) return nullptr;

    auto container = new QWidget();
    auto layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto checkbox = new QCheckBox(normalize_label_text(param_label));
    checkbox->setChecked(value);

    QObject::connect(checkbox, &QCheckBox::toggled, [this](bool v) {
        param_setValue(v);
        param_write_to_repr(value ? "true" : "false");
    });

    layout->addWidget(checkbox);
    return container;
}

void
BoolParam::param_setValue(bool newvalue)
{
    if (value != newvalue) {
        param_effect->refresh_widgets = true;
    }
    value = newvalue;
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */
