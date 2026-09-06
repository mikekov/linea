// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Bryce Harrington <bryce@bryceharrington.org>
 *
 * Copyright (C) 2004 Bryce Harrington
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "unit-menu.h"

#include <QSignalBlocker>

namespace Inkscape::UI::Widget {

UnitMenu::UnitMenu(QWidget* parent)
    : QComboBox(parent)
    , _type(UNIT_TYPE_NONE)
{
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](){ on_changed(); });
}

UnitMenu::~UnitMenu() = default;

bool UnitMenu::setUnitType(UnitType unit_type, bool svg_length)
{
    // Expand the unit widget with unit entries from the unit table
    auto const &unit_table = Util::UnitTable::get();
    auto const &m = unit_table.units(unit_type);

    for (auto & i : m) {
        // We block the use of non SVG units if requested
        if (!svg_length || i->svgUnit() > 0) {
            addItem(QString::fromStdString(i->abbr.raw()));
        }
    }
    _type = unit_type;
    setUnit(unit_table.primary(unit_type));

    return true;
}

bool UnitMenu::resetUnitType(UnitType unit_type, bool svg_length)
{
    clear();

    return setUnitType(unit_type, svg_length);
}

void UnitMenu::addUnit(Unit const& u)
{
    addItem(QString::fromStdString(u.abbr.raw()));
}

Unit const * UnitMenu::getUnit() const
{
    auto const &unit_table = Util::UnitTable::get();
    auto current = get_selected_string();
    if (current.empty()) {
        g_assert(_type != UNIT_TYPE_NONE);
        return unit_table.getUnit(unit_table.primary(_type));
    }
    return unit_table.getUnit(current);
}

bool UnitMenu::setUnit(Glib::ustring const & unit)
{
    // TODO:  Determine if 'unit' is available in the dropdown.
    //        If not, return false

    auto const target = QString::fromStdString(unit.raw());
    int const n = count();
    for (int i = 0; i < n; ++i) {
        if (itemText(i) == target) {
            setCurrentIndex(i);
            break;
        }
    }
    return true;
}

Glib::ustring UnitMenu::getUnitAbbr() const
{
    if (get_selected_string().empty()) {
        return "";
    }
    return getUnit()->abbr;
}

UnitType UnitMenu::getUnitType() const
{
    return getUnit()->type;
}

double  UnitMenu::getUnitFactor() const
{
    return getUnit()->factor;
}

int UnitMenu::getDefaultDigits() const
{
    return getUnit()->defaultDigits();
}

double UnitMenu::getDefaultStep() const
{
    return getUnit()->step;
}

double UnitMenu::getDefaultPage() const
{
    return 10 * getDefaultStep();
}

double UnitMenu::getConversion(Glib::ustring const &new_unit_abbr, Glib::ustring const &old_unit_abbr) const
{
    auto const &unit_table = Util::UnitTable::get();

    double old_factor = getUnit()->factor;
    if (old_unit_abbr != "no_unit") {
        old_factor = unit_table.getUnit(old_unit_abbr)->factor;
    }
    Unit const * new_unit = unit_table.getUnit(new_unit_abbr);

    // Catch the case of zero or negative unit factors (error!)
    if (old_factor < 0.0000001 ||
        new_unit->factor < 0.0000001) {
        // TODO:  Should we assert here?
        return 0.00;
    }

    return old_factor / new_unit->factor;
}

bool UnitMenu::isAbsolute() const
{
    return getUnitType() != UNIT_TYPE_DIMENSIONLESS;
}

bool UnitMenu::isRadial() const
{
    return getUnitType() == UNIT_TYPE_RADIAL;
}

void UnitMenu::on_changed() {
    Q_EMIT changed();
}

Glib::ustring UnitMenu::get_selected_string() const {
    return currentText().toStdString();
}

} // namespace Inkscape::UI::Widget
