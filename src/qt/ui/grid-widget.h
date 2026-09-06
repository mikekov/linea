// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * GridWidget — per-grid settings row shown in the Grids panel
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LINEA_UI_GRID_WIDGET_H
#define LINEA_UI_GRID_WIDGET_H

#include <memory>
#include <vector>
#include <QWidget>

#include "ui/operation-blocker.h"

#include <sigc++/scoped_connection.h>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
namespace Ui {
class GridWidget;
}
QT_END_NAMESPACE

class SPGrid;
namespace Inkscape::XML { class Node; }

namespace Linea::UI {

class AlignmentSelector;
class NumberEdit;
class UnitTracker;

/**
 * Grid widget row for the grids panel (Qt version).
 *
 * Displays and edits a single SPGrid's settings: type, spacing, origin,
 * angles (axonometric), gaps/margins (modular), color, visibility, etc.
 */
class GridWidget : public QWidget {
    Q_OBJECT

public:
    explicit GridWidget(SPGrid* grid, QWidget* parent = nullptr);
    ~GridWidget() override;

    /// Reset this GridWidget to watch a new grid; expects a valid pointer.
    void setGrid(SPGrid* grid);

    SPGrid* grid() const { return _grid; }

Q_SIGNALS:
    void deleteRequested();

private:
    void setupWidgets();
    void connectSignals();
    void watchGrid();
    void update();
    void updateSubordinateWidgets(bool enabled);
    void updateSpinUnits();
    void setSpinValue(NumberEdit* spin, double value_px);
    Inkscape::XML::Node* repr();

    void connectSpin(NumberEdit* spin, const char* prop, bool unitless = false);

    SPGrid* _grid = nullptr;

    std::unique_ptr<Ui::GridWidget> _ui;

    // Unit selector (popup button wired to the tracker)
    UnitTracker* _tracker = nullptr;

    // Alignment popup (created programmatically)
    AlignmentSelector* _alignmentSelector = nullptr;

    // Angle popup entry
    QLineEdit* _aspectRatioEntry = nullptr;

    // Options popover widgets
    QCheckBox* _snapVisibleCheck = nullptr;
    QCheckBox* _dottedCheck = nullptr;
    QCheckBox* _clipToPageCheck = nullptr;

    // Subordinate widgets (disabled when grid is disabled)
    std::vector<QWidget*> _subordinateWidgets;

    // Signal connections
    sigc::scoped_connection _modifiedSignal;

    // Update blocking
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_GRID_WIDGET_H
