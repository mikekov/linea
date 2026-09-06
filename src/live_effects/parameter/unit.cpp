// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "unit.h"

#include <glibmm/i18n.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include "live_effects/effect.h"
#include "util/units.h"

namespace Inkscape::LivePathEffect {

UnitParam::UnitParam(const Glib::ustring& label, const Glib::ustring& tip,
                     const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                     Effect* effect, Glib::ustring default_unit)
    : Parameter(label, tip, key, wr, effect)
    , defunit{default_unit}
{
    unit = std::make_unique<Inkscape::Util::Unit const>(*Util::UnitTable::get().getUnit(default_unit));
}

UnitParam::~UnitParam() = default;

bool
UnitParam::param_readSVGValue(const gchar * strvalue)
{
    if (strvalue) {
        param_set_value(strvalue);
        return true;
    }
    return false;
}

Glib::ustring
UnitParam::param_getSVGValue() const
{
    return unit.get()->abbr;
}

Glib::ustring
UnitParam::param_getDefaultSVGValue() const
{
    return defunit;
}

void
UnitParam::param_set_default()
{
    param_set_value(defunit.c_str());
}

void 
UnitParam::param_update_default(const gchar * default_unit)
{
    defunit = "px"; // fallback to px
    if (default_unit) {
        defunit = default_unit;
    }
}

void
UnitParam::param_set_value(const gchar * strvalue)
{
    if (strvalue) {
        param_effect->refresh_widgets = true;
        unit = std::make_unique<Inkscape::Util::Unit const>(*Util::UnitTable::get().getUnit(strvalue));
    }
}

const gchar *
UnitParam::get_abbreviation() const
{
    return unit.get()->abbr.c_str();
}

QWidget* UnitParam::param_newWidget() {
    auto combo = new QComboBox();
    // Populate with available units
    auto const units_list = Util::UnitTable::get().units(Util::UNIT_TYPE_LINEAR);
    for (auto const* u : units_list) {
        combo->addItem(QString::fromStdString(u->abbr),
                        QString::fromStdString(u->name));
    }
    // Set current unit
    combo->setCurrentText(QString::fromStdString(unit.get()->abbr));

    QObject::connect(combo, &QComboBox::activated, [this, combo](int) {
        auto abbr = combo->currentText().toStdString();
        // Update unit and write to SVG
        param_write_to_repr(abbr.c_str());
    });

    return combo;
}


} // Inkscape::LivePathEffect
