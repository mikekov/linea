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

#include "grid-widget.h"

#include "ui_grid-widget.h"

#include <cmath>
#include <glibmm/i18n.h>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QWidgetAction>

#include "alignment-selector.h"
#include "color-button.h"
#include "number-edit.h"
#include "unit-tracker.h"

#include "document.h"
#include "document-undo.h"
#include "object/sp-grid.h"
#include "util/expression-evaluator.h"
#include "util-string/context-string.h"
#include "util/units.h"
#include "xml/node.h"

using Inkscape::Util::Quantity;

namespace Linea::UI {

namespace {

struct GridTypeInfo {
    const char* label;
    GridType type;
    const char* icon;
};

const GridTypeInfo grid_types[] = {
    {NC_("Grid", "Rectangular"), GridType::RECTANGULAR, "grid-rectangular"},
    {NC_("Grid", "Axonometric"), GridType::AXONOMETRIC, "grid-axonometric"},
    {NC_("Grid", "Modular"),     GridType::MODULAR,     "grid-modular"},
};

} // namespace

Inkscape::XML::Node* GridWidget::repr() {
    return _grid->getRepr();
}

GridWidget::GridWidget(SPGrid* grid, QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::GridWidget>())
    , _grid(grid)
{
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    setupWidgets();
    connectSignals();
    update();
}

GridWidget::~GridWidget() = default;

void GridWidget::connectSpin(NumberEdit* spin, const char* prop, bool unitless) {
    connect(spin, &NumberEdit::valueChanged, this, [this, spin, prop, unitless](double value) {
        if (_update.pending() || !_grid) return;

        auto value_in_px = value;
        if (!unitless) {
            auto current_unit = _tracker->getActiveUnit();
            if (current_unit) {
                value_in_px = Quantity::convert(value, current_unit, "px");
            }
        }
        repr()->setAttributeCssDouble(prop, value_in_px);
        Inkscape::DocumentUndo::done(_grid->document,
            RC_("Undo", "Change grid dimensions"), "grid-dimensions");
    });
}

void GridWidget::setupWidgets() {
    // Populate grid type combo
    for (auto const& gt : grid_types) {
        _ui->gridTypeCombo->addItem(QIcon(":/icons/" + QString(gt.icon)),
                                    QString::fromUtf8(g_dpgettext2(nullptr, "Grid", gt.label)));
    }

    // --- Unit picker: push button that pops up a menu of units ---
    _tracker = new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR, this);
    _tracker->attachPopup(_ui->unitButton);

    // --- Options menu (snap visible, dotted, clip to page) ---
    auto optionsMenu = new QMenu(this);
    auto addCheckboxAction = [this, optionsMenu](const char* text) {
        auto checkbox = new QCheckBox(QString::fromUtf8(_(text)), this);
        checkbox->setStyleSheet("QCheckBox { padding: 4px 8px; }");
        auto action = new QWidgetAction(optionsMenu);
        action->setDefaultWidget(checkbox);
        optionsMenu->addAction(action);
        return checkbox;
    };

    _snapVisibleCheck = addCheckboxAction("Snap to visible grid lines only");
    _dottedCheck = addCheckboxAction("Show dots instead of lines");
    _clipToPageCheck = addCheckboxAction("Clip to page");

    connect(_ui->optionsButton, &QPushButton::clicked, this, [this, optionsMenu]() {
        optionsMenu->popup(_ui->optionsButton->mapToGlobal(_ui->optionsButton->rect().bottomLeft()));
    });

    // --- Angle popup menu (aspect ratio entry) ---
    auto angleMenu = new QMenu(this);
    auto angleWidget = new QWidget(angleMenu);
    auto angleLayout = new QHBoxLayout(angleWidget);
    angleLayout->setContentsMargins(8, 4, 8, 4);
    _aspectRatioEntry = new QLineEdit(angleWidget);
    _aspectRatioEntry->setPlaceholderText("W : H");
    angleLayout->addWidget(_aspectRatioEntry);
    auto applyAngleBtn = new QPushButton(QString::fromUtf8(_("Set")), angleWidget);
    angleLayout->addWidget(applyAngleBtn);
    auto angleAction = new QWidgetAction(angleMenu);
    angleAction->setDefaultWidget(angleWidget);
    angleMenu->addAction(angleAction);

    connect(_ui->anglePopupButton, &QPushButton::clicked, this, [this, angleMenu]() {
        angleMenu->popup(_ui->anglePopupButton->mapToGlobal(_ui->anglePopupButton->rect().bottomLeft()));
    });

    connect(applyAngleBtn, &QPushButton::clicked, this, [this]() {
        if (!_grid) return;
        try {
            auto text = _aspectRatioEntry->text();
            auto result = Inkscape::Util::ExpressionEvaluator(text.toUtf8().constData()).evaluate().value;
            if (!std::isfinite(result) || result <= 0) return;
            auto ang = Geom::deg_from_rad(std::atan(1.0 / result));
            if (ang > 0.0 && ang < 90.0) {
                _ui->angleX->setValue(ang);
                _ui->angleZ->setValue(ang);
                if (_grid->document) {
                    Inkscape::DocumentUndo::done(_grid->document,
                        RC_("Undo", "Change grid dimensions"), "grid-angle");
                }
            }
        }
        catch (Inkscape::Util::EvaluatorException&) {
            // Ignore invalid expressions
            printf("Axonometrix grid angles: Invalid expression\n");
        }
    });

    connect(angleMenu, &QMenu::aboutToShow, this, [this]() {
        if (!_grid) return;
        auto ax = _ui->angleX->value();
        auto az = _ui->angleZ->value();
        if (az == ax) {
            auto ratio = std::tan(Geom::rad_from_deg(ax));
            if (ratio > 0) {
                _aspectRatioEntry->setText(ratio > 1.0
                    ? QString("1 : %1").arg(ratio)
                    : QString("%1 : 1").arg(1.0 / ratio));
            }
        }
    });

    // --- Alignment popup ---
    auto alignMenu = new QMenu(this);
    _alignmentSelector = new AlignmentSelector();
    auto alignAction = new QWidgetAction(alignMenu);
    alignAction->setDefaultWidget(_alignmentSelector);
    alignMenu->addAction(alignAction);

    connect(_ui->alignButton, &QPushButton::clicked, this, [this, alignMenu]() {
        alignMenu->popup(_ui->alignButton->mapToGlobal(_ui->alignButton->rect().bottomLeft()));
    });

    // --- Collect subordinate widgets ---
    _subordinateWidgets = {
        _ui->originX, _ui->originY,
        _ui->spacingX, _ui->spacingY,
        _ui->gapX, _ui->gapY,
        _ui->marginX, _ui->marginY,
        _ui->angleX, _ui->angleZ,
        _ui->noOfLines,
        _ui->originLabel, _ui->spacingLabel, _ui->gapLabel,
        _ui->marginLabel, _ui->angleLabel, _ui->noOfLinesLabel,
        _ui->typeLabel, _ui->colorLabel,
        _ui->gridTypeCombo,
        _ui->visibleToggle, _ui->colorButton, _ui->optionsButton,
        _ui->alignButton, _ui->anglePopupButton, _ui->unitButton,
        _snapVisibleCheck, _dottedCheck, _clipToPageCheck,
        _alignmentSelector,
    };
}

void GridWidget::connectSignals() {
    // Grid type dropdown
    connect(_ui->gridTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (_update.pending() || index < 0 || index >= 3) return;
        _grid->setType(grid_types[index].type);
        update();
    });

    // Delete button — defer deletion so that the resource-changed signal
    // (which rebuilds the panel) doesn't destroy us mid-handler.
    connect(_ui->deleteButton, &QPushButton::clicked, this, [this]() {
        if (!_grid) return;

        auto grid = _grid;
        _grid = nullptr;
        QTimer::singleShot(0, [grid]() {
            if (!grid || !grid->document) return;
            auto doc = grid->document;
            grid->deleteObject();
            Inkscape::DocumentUndo::done(doc, RC_("Undo", "Remove grid"), "");
        });
    });

    // Enabled switch
    connect(_ui->enabledCheck, &SwitchWidget::toggled, this, [this](bool enabled) {
        if (_update.pending()) return;

        repr()->setAttributeBoolean("enabled", enabled);
        updateSubordinateWidgets(enabled);
        if (_grid && _grid->document) {
            Inkscape::DocumentUndo::done(_grid->document,
                RC_("Undo", "Change grid enabled state"), "grid-enabled");
        }
    });

    // Visible toggle
    connect(_ui->visibleToggle, &QPushButton::toggled, this, [this](bool visible) {
        if (_update.pending()) return;

        repr()->setAttributeBoolean("visible", visible);
        if (_grid && _grid->document) {
            Inkscape::DocumentUndo::done(_grid->document,
                RC_("Undo", "Change grid visibility"), "grid-visible");
        }
    });

    // Spin buttons
    connectSpin(_ui->originX, "originx");
    connectSpin(_ui->originY, "originy");
    connectSpin(_ui->spacingX, "spacingx");
    connectSpin(_ui->spacingY, "spacingy");
    connectSpin(_ui->angleX, "gridanglex", true);
    connectSpin(_ui->angleZ, "gridanglez", true);
    connectSpin(_ui->noOfLines, "empspacing", true);
    connectSpin(_ui->gapX, "gapx");
    connectSpin(_ui->gapY, "gapy");
    connectSpin(_ui->marginX, "marginx");
    connectSpin(_ui->marginY, "marginy");

    // Color button
    _ui->colorButton->setTitle(tr("Grid lines"));
    connect(_ui->colorButton, &ColorPicker::colorChanged, this, [this](const Inkscape::Colors::Color& inkColor) {
        if (!_grid) return;

        repr()->setAttribute("empcolor", inkColor.toString(false).c_str());
        repr()->setAttributeCssDouble("empopacity", inkColor.getOpacity());

        auto colorWithOpacity = inkColor;
        colorWithOpacity.addOpacity(0.5);
        repr()->setAttribute("color", colorWithOpacity.toString(false).c_str());
        repr()->setAttributeCssDouble("opacity", colorWithOpacity.getOpacity());

        if (_grid->document) {
            Inkscape::DocumentUndo::done(_grid->document,
                RC_("Undo", "Change grid color"), "grid-color");
        }
        update();
    });

    // Options checkboxes
    connect(_snapVisibleCheck, &QCheckBox::toggled, this, [this](bool active) {
        if (_update.pending()) return;
        repr()->setAttributeBoolean("snapvisiblegridlinesonly", active);
        if (_grid && _grid->document) {
            Inkscape::DocumentUndo::done(_grid->document,
                RC_("Undo", "Change grid snap settings"), "grid-snap");
        }
    });

    connect(_dottedCheck, &QCheckBox::toggled, this, [this](bool active) {
        if (_update.pending()) return;
        repr()->setAttributeBoolean("dotted", active);
        if (_grid && _grid->document) {
            Inkscape::DocumentUndo::done(_grid->document,
                RC_("Undo", "Change grid dotted setting"), "grid-dotted");
        }
    });

    connect(_clipToPageCheck, &QCheckBox::toggled, this, [this](bool active) {
        if (_update.pending()) return;
        repr()->setAttributeBoolean("cliptopage", active);
        if (_grid && _grid->document) {
            Inkscape::DocumentUndo::done(_grid->document,
                RC_("Undo", "Change grid clip-to-page setting"), "grid-clip-to-page");
        }
    });

    // Alignment selector
    connect(_alignmentSelector, &AlignmentSelector::alignmentClicked,
            this, [this](int align) {
        if (_update.pending() || !_grid) return;

        auto dimensions = _grid->document->getDimensions();
        dimensions[Geom::X] *= align % 3 * 0.5;
        dimensions[Geom::Y] *= align / 3 * 0.5;
        dimensions *= _grid->document->doc2dt();
        dimensions *= _grid->document->getDocumentScale().inverse();
        _grid->setOrigin(dimensions);
        update();
    });

    // Unit changes — update() re-reads all values from the grid with proper conversion
    connect(_tracker, &UnitTracker::unitChanged, this, [this](const Inkscape::Util::Unit* unit) {
        if (_update.pending()) return;
        if (_grid && unit) {
            _grid->setUnit(unit->abbr);
            update();
        }
        updateSpinUnits();
    });
    _tracker->setActiveUnitByAbbr("px");
    updateSpinUnits();

    watchGrid();
}

void GridWidget::watchGrid() {
    _modifiedSignal = _grid->connectModified([this](const SPObject*, unsigned) {
        if (!_update.pending()) {
            _modifiedSignal.block();
            update();
            _modifiedSignal.unblock();
        }
    });
}

void GridWidget::setGrid(SPGrid* grid) {
    if (!grid) return;

    _grid = grid;
    watchGrid();
    update();
}

void GridWidget::update() {
    if (!_grid) return;

    auto block = _update.block();

    // Set the active unit from the grid
    _tracker->setActiveUnit(_grid->getUnit());
    // Sync spin-box suffixes to the tracker's unit; the unitChanged handler
    // is blocked by _update above, so updateSpinUnits() won't run otherwise.
    updateSpinUnits();

    const auto modular     = _grid->getType() == GridType::MODULAR;
    const auto axonometric = _grid->getType() == GridType::AXONOMETRIC;
    const auto rectangular = _grid->getType() == GridType::RECTANGULAR;

    // Grid type dropdown
    {
        QSignalBlocker blocker(_ui->gridTypeCombo);
        _ui->gridTypeCombo->setCurrentIndex(static_cast<int>(_grid->getType()));
    }

    // Origin (getOrigin returns px; setSpinValue converts px → display unit)
    auto origin = _grid->getOrigin();
    setSpinValue(_ui->originX, origin[Geom::X]);
    setSpinValue(_ui->originY, origin[Geom::Y]);

    // Spacing (getSpacing returns px; setSpinValue converts px → display unit)
    auto spacing = _grid->getSpacing();
    setSpinValue(_ui->spacingX, spacing[Geom::X]);
    setSpinValue(_ui->spacingY, spacing[Geom::Y]);

    // Spacing label depends on grid type
    _ui->spacingLabel->setText(QString::fromUtf8(modular ? _("Block size") : _("Spacing")));
    _ui->spacingX->setLabel(modular ? "W" : "X");
    _ui->spacingX->setToolTip(QString::fromUtf8(modular
        ? _("Width of grid modules")
        : _("Distance between vertical grid lines")));
    _ui->spacingY->setLabel(modular ? "H" : "Y");
    _ui->spacingY->setToolTip(QString::fromUtf8(modular
        ? _("Height of grid modules")
        : _("Distance between horizontal grid lines")));

    // Visibility based on grid type
    _ui->angleX->setVisible(axonometric);
    _ui->angleZ->setVisible(axonometric);
    _ui->angleLabel->setVisible(axonometric);
    _ui->anglePopupButton->setVisible(axonometric);

    if (axonometric) {
        _ui->angleX->setValue(_grid->getAngleX());
        _ui->angleZ->setValue(_grid->getAngleZ());
    }

    _ui->gapX->setVisible(modular);
    _ui->gapY->setVisible(modular);
    _ui->gapLabel->setVisible(modular);
    _ui->marginX->setVisible(modular);
    _ui->marginY->setVisible(modular);
    _ui->marginLabel->setVisible(modular);

    if (modular) {
        auto gap    = _grid->get_gap();
        auto margin = _grid->get_margin();
        setSpinValue(_ui->gapX, gap.x());
        setSpinValue(_ui->gapY, gap.y());
        setSpinValue(_ui->marginX, margin.x());
        setSpinValue(_ui->marginY, margin.y());
    }

    // Color button — update swatch
    _ui->colorButton->setColor(_grid->getMajorColor());

    _ui->noOfLines->setVisible(!modular);
    _ui->noOfLinesLabel->setVisible(!modular);
    _ui->noOfLines->setValue(_grid->getMajorLineInterval());

    // Switches
    {
        QSignalBlocker b1(_ui->enabledCheck);
        _ui->enabledCheck->setChecked(_grid->isEnabled());
    }
    {
        QSignalBlocker b2(_ui->visibleToggle);
        _ui->visibleToggle->setChecked(_grid->isVisible());
    }
    {
        QSignalBlocker b3(_dottedCheck);
        _dottedCheck->setChecked(_grid->isDotted());
    }
    {
        QSignalBlocker b4(_snapVisibleCheck);
        _snapVisibleCheck->setChecked(_grid->getSnapToVisibleOnly());
    }
    {
        QSignalBlocker b5(_clipToPageCheck);
        _clipToPageCheck->setChecked(_grid->isClipToPage());
    }

    // Dotted checkbox only relevant for rectangular
    _dottedCheck->setVisible(rectangular);

    // Spacing X hidden for axonometric
    _ui->spacingX->setVisible(!axonometric);

    updateSubordinateWidgets(_grid->isEnabled());

    // ID label
    auto id = _grid->getId() ? _grid->getId() : "-";
    _ui->idLabel->setText(QString::fromUtf8(id));
    _ui->idLabel->setToolTip(QString::fromUtf8(id));
}

void GridWidget::updateSubordinateWidgets(bool enabled) {
    for (auto widget : _subordinateWidgets) {
        widget->setEnabled(enabled);
    }
}

void GridWidget::updateSpinUnits() {
    auto unit = _tracker->getActiveUnit();
    if (!unit) return;

    auto abbr = QString(" %1").arg(QString::fromStdString(unit->abbr.raw()));
    _ui->spacingX->setSuffix(abbr);
    _ui->spacingY->setSuffix(abbr);
    _ui->gapX->setSuffix(abbr);
    _ui->gapY->setSuffix(abbr);
    _ui->marginX->setSuffix(abbr);
    _ui->marginY->setSuffix(abbr);
    _ui->originX->setSuffix(abbr);
    _ui->originY->setSuffix(abbr);
}

void GridWidget::setSpinValue(NumberEdit* spin, double value_px) {
    auto current_unit = _tracker->getActiveUnit();
    if (current_unit) {
        double value_display = Quantity::convert(value_px, "px", current_unit);
        spin->setValue(value_display);
    } else {
        spin->setValue(value_px);
    }
}

} // namespace Linea::UI
