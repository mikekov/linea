// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * UnitTracker — synchronizes unit changes across spin boxes (Qt version).
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "unit-tracker.h"

#include <QComboBox>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <iostream>
#include <utility>

#include "number-edit.h"

namespace Linea::UI {

UnitTracker::UnitTracker(UnitType unit_type, QObject* parent)
    : QObject(parent) {
    const auto& table = UnitTable::get();
    for (const auto* u : table.units(unit_type)) {
        _units.push_back({u->abbr.raw(), u, nullptr});
    }
    setActive(0);
}

UnitTracker::~UnitTracker() = default;

bool UnitTracker::isUpdating() const {
    return _isUpdating;
}

const Unit* UnitTracker::getActiveUnit() const {
    return _activeUnit;
}

void UnitTracker::setActiveUnit(const Unit* unit) {
    if (!unit) return;

    for (int i = 0; i < static_cast<int>(_units.size()); ++i) {
        if (_units[i].abbr == unit->abbr.raw()) {
            setActive(i);
            return;
        }
    }
    std::cerr << "UnitTracker::setActiveUnit: unit '" << unit->abbr << "' not found!" << std::endl;
}

void UnitTracker::setActiveUnitByAbbr(const char* abbr) {
    if (!abbr) return;

    const auto& table = UnitTable::get();
    auto unit = table.getUnit(abbr);
    if (unit && unit->abbr == abbr) {
        setActiveUnit(unit);
    } else {
        // Try a direct search in our list
        for (int i = 0; i < static_cast<int>(_units.size()); ++i) {
            if (_units[i].abbr == abbr) {
                setActive(i);
                return;
            }
        }
        std::cerr << "UnitTracker::setActiveUnitByAbbr: '" << abbr << "' not found" << std::endl;
    }
}

void UnitTracker::addSpinBox(NumberEdit* spin) {
    if (!spin) return;

    if (std::find(_spinBoxes.begin(), _spinBoxes.end(), spin) != _spinBoxes.end()) return;

    _spinBoxes.push_back(spin);

    // Remove from list if the spin box is destroyed
    connect(spin, &QObject::destroyed, this, [this, spin]() { removeSpinBox(spin); });
}

void UnitTracker::removeSpinBox(NumberEdit* spin) {
    auto it = std::find(_spinBoxes.begin(), _spinBoxes.end(), spin);
    if (it != _spinBoxes.end()) {
        _spinBoxes.erase(it);
        _priorValues.erase(spin);
    }
}

void UnitTracker::addUnit(const Unit* unit) {
    if (!unit) return;
    _units.push_back({unit->abbr.raw(), unit, nullptr});
    for (auto combo : _combos) {
        combo->addItem(QString::fromStdString(unit->abbr.raw()));
    }
}

void UnitTracker::addUnit(std::unique_ptr<const Unit> unit) {
    if (!unit) return;
    const Unit* ptr = unit.get();
    _units.push_back({ptr->abbr.raw(), ptr, std::move(unit)});
    for (auto combo : _combos) {
        combo->addItem(QString::fromStdString(ptr->abbr.raw()));
    }
}

void UnitTracker::prependUnit(const Unit* unit) {
    if (!unit) return;
    _units.insert(_units.begin(), {unit->abbr.raw(), unit, nullptr});
    for (auto combo : _combos) {
        combo->insertItem(0, QString::fromStdString(unit->abbr.raw()));
    }
    // keep the active index pointing to the same unit
    ++_active;
    for (auto combo : _combos) {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(_active);
    }
}

void UnitTracker::prependUnit(std::unique_ptr<const Unit> unit) {
    if (!unit) return;
    const Unit* ptr = unit.get();
    _units.insert(_units.begin(), {ptr->abbr.raw(), ptr, std::move(unit)});
    for (auto combo : _combos) {
        combo->insertItem(0, QString::fromStdString(ptr->abbr.raw()));
    }
    // keep the active index pointing to the same unit
    ++_active;
    for (auto combo : _combos) {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(_active);
    }
}

QComboBox* UnitTracker::createUnitCombo(QWidget* parent) {
    auto combo = new QComboBox(parent);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    attachCombo(combo);
    return combo;
}

void UnitTracker::attachCombo(QComboBox* combo) {
    if (!combo) return;

    // Avoid double-registering the same combo.
    if (std::find(_combos.begin(), _combos.end(), combo) != _combos.end()) return;

    for (const auto& entry : _units) {
        combo->addItem(QString::fromStdString(entry.abbr));
    }
    combo->setCurrentIndex(_active);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) { setActive(index); });

    // Remove from list if combo is destroyed
    connect(combo, &QObject::destroyed, this, [this, combo]() {
        auto it = std::find(_combos.begin(), _combos.end(), combo);
        if (it != _combos.end()) _combos.erase(it);
    });

    _combos.push_back(combo);
}

QMenu* UnitTracker::attachPopup(QPushButton* button) {
    if (!button) return nullptr;

    auto menu = new QMenu(button);
    for (int i = 0; i < static_cast<int>(_units.size()); ++i) {
        auto action = menu->addAction(QString::fromStdString(_units[i].abbr));
        action->setCheckable(true);
        action->setChecked(i == _active);
        connect(action, &QAction::triggered, this, [this, i]() { setActive(i); });
    }

    connect(button, &QPushButton::clicked, this,
            [button, menu]() { menu->popup(button->mapToGlobal(button->rect().bottomLeft())); });

    // Remove from list if menu is destroyed
    connect(menu, &QObject::destroyed, this, [this, menu]() {
        auto it = std::find(_popupMenus.begin(), _popupMenus.end(), menu);
        if (it != _popupMenus.end()) _popupMenus.erase(it);
    });

    _popupMenus.push_back(menu);
    return menu;
}

void UnitTracker::setActive(int index) {
    if (index < 0 || index >= static_cast<int>(_units.size())) return;
    if (index == _active && _activeUnitInitialized) return;

    auto oldUnit = _activeUnit;
    auto newUnit = _units[index].unit;

    _activeUnit = newUnit;
    _active = index;
    _activeUnitInitialized = true;

    if (oldUnit && newUnit && !_spinBoxes.empty()) {
        fixupSpinBoxes(oldUnit, newUnit);
    }

    // Sync combo boxes
    for (auto combo : _combos) {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(index);
    }

    // Sync popup menus
    for (auto menu : _popupMenus) {
        auto actions = menu->actions();
        for (int i = 0; i < actions.size() && i < static_cast<int>(_units.size()); ++i) {
            actions[i]->setChecked(i == index);
        }
    }

    Q_EMIT unitChanged(_activeUnit);
}

void UnitTracker::fixupSpinBoxes(const Unit* oldUnit, const Unit* newUnit) {
    _isUpdating = true;

    for (auto spin : _spinBoxes) {
        double oldVal = spin->value();
        double val = oldVal;

        if (oldUnit->type != Inkscape::Util::UNIT_TYPE_DIMENSIONLESS &&
            newUnit->type == Inkscape::Util::UNIT_TYPE_DIMENSIONLESS) {
            val = newUnit->factor * 100;
            _priorValues[spin] = Quantity::convert(oldVal, oldUnit, "px");
        } else if (oldUnit->type == Inkscape::Util::UNIT_TYPE_DIMENSIONLESS &&
                   newUnit->type != Inkscape::Util::UNIT_TYPE_DIMENSIONLESS) {
            auto it = _priorValues.find(spin);
            if (it != _priorValues.end()) {
                val = Quantity::convert(it->second, "px", newUnit);
            }
        } else {
            val = Quantity::convert(oldVal, oldUnit, newUnit);
        }

        QSignalBlocker blocker(spin);
        spin->setValue(val);
    }

    _isUpdating = false;
}

} // namespace Linea::UI
