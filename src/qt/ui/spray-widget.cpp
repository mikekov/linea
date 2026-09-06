// SPDX-License-Identifier: GPL-2.0-or-later
/** @file SprayWidget implementation. */

#include "spray-widget.h"

#include <QButtonGroup>
#include <QPushButton>
#include <algorithm>
#include <array>

#include "number-edit.h"
#include "preferences.h"
#include "ui_spray-widget.h"

namespace Linea::UI {

using Inkscape::Preferences;

SprayWidget::SprayWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::SprayWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto configure = [](NumberEdit* spin, const char* path, double value, double minimum, double maximum, int decimals,
                        double step = 1) {
        spin->setRange(minimum, maximum);
        spin->setDecimals(decimals);
        spin->setValue(Preferences::get()->getDouble(path, value));
        spin->setSingleStep(step);
        QObject::connect(spin, &NumberEdit::valueChanged, spin,
                         [path](double value) { Preferences::get()->setDouble(path, value); });
    };
    configure(_ui->widthSpin, "/tools/spray/width", 15, 1, 100, 0);
    configure(_ui->populationSpin, "/tools/spray/population", 70, 1, 100, 0);
    configure(_ui->rotationSpin, "/tools/spray/rotation_variation", 0, 0, 100, 0);
    configure(_ui->scaleSpin, "/tools/spray/scale_variation", 0, 0, 100, 0);
    configure(_ui->scatterSpin, "/tools/spray/standard_deviation", 70, 1, 100, 0);
    configure(_ui->focusSpin, "/tools/spray/mean", 0, 0, 100, 0);
    configure(_ui->offsetSpin, "/tools/spray/offset", 100, 0, 1000, 0);

    struct PressureControl {
        QPushButton* button;
        const char* path;
        bool defaultValue;
    };
    const std::array pressureData = {
        PressureControl{_ui->pressureWidthButton, "/tools/spray/usepressurewidth", false},
        PressureControl{_ui->pressurePopulationButton, "/tools/spray/usepressurepopulation", false},
        PressureControl{_ui->pressureScaleButton, "/tools/spray/usepressurescale", false},
    };
    for (const auto& [button, path, defaultValue] : pressureData) {
        button->setChecked(Preferences::get()->getBool(path, defaultValue));
        QObject::connect(button, &QPushButton::toggled, button,
                         [path](bool value) { Preferences::get()->setBool(path, value); });
    }
    auto updateScaleEnabled = [this](bool pressureScale) {
        if (pressureScale) {
            _ui->scaleSpin->setValue(0);
        }
        _ui->scaleSpin->setEnabled(!pressureScale);
    };
    QObject::connect(_ui->pressureScaleButton, &QPushButton::toggled, this, updateScaleEnabled);
    updateScaleEnabled(_ui->pressureScaleButton->isChecked());

    const std::array buttons = {_ui->copyButton, _ui->cloneButton, _ui->unionButton, _ui->deleteButton};
    auto modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        modeGroup->addButton(buttons[i], i);
    }
    const auto mode = std::clamp(Preferences::get()->getInt("/tools/spray/mode", 1), 0, 3);
    buttons[mode]->setChecked(true);

    auto configureOption = [](QPushButton* button, const char* path, bool defaultValue) {
        button->setChecked(Preferences::get()->getBool(path, defaultValue));
        QObject::connect(button, &QPushButton::toggled, button,
                         [path](bool value) { Preferences::get()->setBool(path, value); });
    };
    configureOption(_ui->overNoTransparentButton, "/tools/spray/over_no_transparent", true);
    configureOption(_ui->overTransparentButton, "/tools/spray/over_transparent", true);
    configureOption(_ui->pickNoOverlapButton, "/tools/spray/pick_no_overlap", false);
    configureOption(_ui->noOverlapButton, "/tools/spray/no_overlap", false);
    configureOption(_ui->pickerButton, "/tools/spray/picker", false);
    configureOption(_ui->pickFillButton, "/tools/spray/pick_fill", false);
    configureOption(_ui->pickStrokeButton, "/tools/spray/pick_stroke", false);
    configureOption(_ui->pickInverseValueButton, "/tools/spray/pick_inverse_value", false);
    configureOption(_ui->pickCenterButton, "/tools/spray/pick_center", true);

    auto updateVisibility = [this](int id) {
        const bool showGeneral = id != 2 && id != 3;
        const bool showRotation = id != 3;
        const std::array<QWidget*, 2> rotationWidgets = {_ui->rotationLabel, _ui->rotationSpin};
        for (auto widget : rotationWidgets) {
            widget->setVisible(showRotation);
        }
        const std::array<QWidget*, 6> generalWidgets = {
            _ui->optionsLabel,
            _ui->overNoTransparentButton,
            _ui->overTransparentButton,
            _ui->pickNoOverlapButton,
            _ui->noOverlapButton,
            _ui->offsetSpin,
        };
        for (auto widget : generalWidgets) {
            widget->setVisible(showGeneral);
        }
        _ui->offsetSpin->setEnabled(showGeneral && _ui->noOverlapButton->isChecked());
        _ui->pickerButton->setVisible(showGeneral);
        const bool picker = showGeneral && _ui->pickerButton->isChecked();
        for (auto widget :
             {_ui->pickFillButton, _ui->pickStrokeButton, _ui->pickInverseValueButton, _ui->pickCenterButton})
            widget->setVisible(picker);
    };
    QObject::connect(modeGroup, &QButtonGroup::idClicked, this, [modeGroup, updateVisibility](int id) {
        Preferences::get()->setInt("/tools/spray/mode", id);
        updateVisibility(id);
    });
    QObject::connect(_ui->pickerButton, &QPushButton::toggled, this,
                     [modeGroup, updateVisibility](bool) { updateVisibility(modeGroup->checkedId()); });
    QObject::connect(_ui->noOverlapButton, &QPushButton::toggled, this,
                     [modeGroup, updateVisibility](bool) { updateVisibility(modeGroup->checkedId()); });
    updateVisibility(mode);
}

SprayWidget::~SprayWidget() = default;

void SprayWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

} // namespace Linea::UI
