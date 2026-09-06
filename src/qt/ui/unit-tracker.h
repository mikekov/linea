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

#ifndef LINEA_UI_UNIT_TRACKER_H
#define LINEA_UI_UNIT_TRACKER_H

#include <QObject>
#include <map>
#include <memory>
#include <vector>

#include "util/units.h"

class QComboBox;
class QMenu;
class QPushButton;

namespace Linea::UI {

class NumberEdit;

using Inkscape::Util::Quantity;
using Inkscape::Util::Unit;
using Inkscape::Util::UnitTable;
using Inkscape::Util::UnitType;

/**
 * Simple mediator to synchronize unit changes across spin boxes and combo boxes.
 *
 * When the active unit changes, all registered spin boxes have their values
 * converted from the old unit to the new one. Combo boxes created via
 * createUnitCombo() are kept in sync with the active selection.
 */
class UnitTracker : public QObject {
    Q_OBJECT

public:
    explicit UnitTracker(UnitType unit_type, QObject* parent = nullptr);
    ~UnitTracker() override;

    bool isUpdating() const;

    void setActiveUnit(const Unit* unit);
    void setActiveUnitByAbbr(const char* abbr);
    const Unit* getActiveUnit() const;

    void addSpinBox(NumberEdit* spin);
    void removeSpinBox(NumberEdit* spin);

    /// Append a unit to the tracker's list (e.g., custom units like "lines").
    void addUnit(const Unit* unit);
    void addUnit(std::unique_ptr<const Unit> unit);
    /// Prepend a unit to the tracker's list.
    void prependUnit(const Unit* unit);
    void prependUnit(std::unique_ptr<const Unit> unit);

    /// Create a QComboBox populated with units and wired to this tracker.
    QComboBox* createUnitCombo(QWidget* parent = nullptr);

    /// Populate and wire an existing QComboBox to this tracker.
    void attachCombo(QComboBox* combo);

    /// Create a popup menu of units and wire it to the given button's click.
    /// The button's text is not modified — the caller should update it via
    /// the unitChanged signal. Returns the created menu.
    QMenu* attachPopup(QPushButton* button);

Q_SIGNALS:
    void unitChanged(const Unit* unit);

private:
    void setActive(int index);
    void fixupSpinBoxes(const Unit* oldUnit, const Unit* newUnit);

    struct UnitEntry {
        std::string abbr;
        const Unit* unit;
        std::unique_ptr<const Unit> owned; // keeps temporary/custom units alive
    };

    int _active = 0;
    bool _isUpdating = false;
    const Unit* _activeUnit = nullptr;
    bool _activeUnitInitialized = false;

    std::vector<UnitEntry> _units;
    std::vector<QComboBox*> _combos;
    std::vector<QMenu*> _popupMenus;
    std::vector<NumberEdit*> _spinBoxes;
    std::map<NumberEdit*, double> _priorValues;
};

} // namespace Linea::UI

#endif // LINEA_UI_UNIT_TRACKER_H
