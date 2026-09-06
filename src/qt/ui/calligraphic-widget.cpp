// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * CalligraphicWidget implementation.
 */

#include "calligraphic-widget.h"

#include <QPushButton>

#include "icon-combobox.h"
#include "number-edit.h"
#include "preferences.h"
#include "ui_calligraphic-widget.h"

namespace Linea::UI {

using Inkscape::Preferences;

CalligraphicWidget::CalligraphicWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::CalligraphicWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto configure = [](NumberEdit* spin, const char* path, double value, double minimum, double maximum, int decimals, double step = 1) {
        spin->setRange(minimum, maximum);
        spin->setDecimals(decimals);
        spin->setValue(Preferences::get()->getDouble(path, value));
        spin->setSingleStep(step);
        QObject::connect(spin, &NumberEdit::valueChanged, spin, [path](double value) {
            Preferences::get()->setDouble(path, value);
        });
    };
    configure(_ui->widthSpin, "/tools/calligraphic/width", 15.118, 0.001, 100, 3);
    configure(_ui->thinningSpin, "/tools/calligraphic/thinning", 10, -100, 100, 1);
    configure(_ui->massSpin, "/tools/calligraphic/mass", 2, 0, 100, 1);
    configure(_ui->angleSpin, "/tools/calligraphic/angle", 30, -90, 90, 1);
    configure(_ui->flatnessSpin, "/tools/calligraphic/flatness", 90, -100, 100, 1);
    configure(_ui->capRoundingSpin, "/tools/calligraphic/cap_rounding", 0, 0, 5, 2, 0.01);
    configure(_ui->tremorSpin, "/tools/calligraphic/tremor", 0, 0, 100, 1);
    configure(_ui->wiggleSpin, "/tools/calligraphic/wiggle", 0, 0, 100, 1);

    auto configureCheck = [](QPushButton* check, const char* path, bool value) {
        check->setChecked(Preferences::get()->getBool(path, value));
        QObject::connect(check, &QPushButton::toggled, check, [path](bool value) {
            Preferences::get()->setBool(path, value);
        });
    };
    configureCheck(_ui->pressureCheck, "/tools/calligraphic/usepressure", false);
    configureCheck(_ui->tiltCheck, "/tools/calligraphic/usetilt", true);
    configureCheck(_ui->traceBackgroundCheck, "/tools/calligraphic/tracebackground", false);

    auto updateAngleEnabled = [this](bool tilt) { _ui->angleSpin->setEnabled(!tilt); };
    QObject::connect(_ui->tiltCheck, &QPushButton::toggled, this, updateAngleEnabled);
    updateAngleEnabled(_ui->tiltCheck->isChecked());

    _ui->profileCombo->setHeaderType(IconComboBox::LabelOnly);
    populatePresets();
    QObject::connect(_ui->profileCombo, &IconComboBox::currentChanged, this, &CalligraphicWidget::applyPreset);
}

CalligraphicWidget::~CalligraphicWidget() = default;

void CalligraphicWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void CalligraphicWidget::populatePresets() {
    _presetPaths.clear();
    _ui->profileCombo->addRow({}, tr("No preset"), 0);

    for (const auto& preset : Preferences::get()->getAllDirs("/tools/calligraphic/preset")) {
        auto name = Preferences::get()->getString(preset + "/name");
        if (name.empty()) continue;

        _presetPaths.emplace_back(preset);
        _ui->profileCombo->addRow({}, QString::fromUtf8(name.c_str()), static_cast<int>(_presetPaths.size()));
    }
}

void CalligraphicWidget::applyPreset(int id) {
    if (id <= 0 || id > static_cast<int>(_presetPaths.size())) return;

    const auto path = _presetPaths[id - 1];
    for (const auto& entry : Preferences::get()->getAllEntries(path)) {
        const auto name = entry.getEntryName();
        if (name == "id" || name == "name") continue;

        if (name == "width") _ui->widthSpin->setValue(entry.getDouble());
        else if (name == "thinning") _ui->thinningSpin->setValue(entry.getDouble());
        else if (name == "mass") _ui->massSpin->setValue(entry.getDouble());
        else if (name == "angle") _ui->angleSpin->setValue(entry.getDouble());
        else if (name == "flatness") _ui->flatnessSpin->setValue(entry.getDouble());
        else if (name == "cap_rounding") _ui->capRoundingSpin->setValue(entry.getDouble());
        else if (name == "tremor") _ui->tremorSpin->setValue(entry.getDouble());
        else if (name == "wiggle") _ui->wiggleSpin->setValue(entry.getDouble());
        else if (name == "usepressure") _ui->pressureCheck->setChecked(entry.getBool());
        else if (name == "usetilt") _ui->tiltCheck->setChecked(entry.getBool());
        else if (name == "tracebackground") _ui->traceBackgroundCheck->setChecked(entry.getBool());
    }
}

} // namespace Linea::UI
