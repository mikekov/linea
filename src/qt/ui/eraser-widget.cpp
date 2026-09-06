// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * EraserWidget implementation.
 */

#include "eraser-widget.h"

#include <QButtonGroup>
#include <QPushButton>
#include <algorithm>

#include "number-edit.h"
#include "preferences.h"
#include "ui_eraser-widget.h"

namespace Linea::UI {

using Inkscape::Preferences;

EraserWidget::EraserWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::EraserWidget>()) {
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
    configure(_ui->widthSpin, "/tools/eraser/width", 15, 0, 100, 1);
    configure(_ui->thinningSpin, "/tools/eraser/thinning", 10, -100, 100, 1);
    configure(_ui->capRoundingSpin, "/tools/eraser/cap_rounding", 0, 0, 5, 2, 0.01);
    configure(_ui->tremorSpin, "/tools/eraser/tremor", 0, 0, 100, 1);
    configure(_ui->massSpin, "/tools/eraser/mass", 10, 0, 100, 1);

    auto configureCheck = [](QPushButton* check, const char* path, bool value) {
        check->setChecked(Preferences::get()->getBool(path, value));
        QObject::connect(check, &QPushButton::toggled, check,
                         [path](bool value) { Preferences::get()->setBool(path, value); });
    };
    configureCheck(_ui->pressureCheck, "/tools/eraser/usepressure", false);
    configureCheck(_ui->splitCheck, "/tools/eraser/break_apart", false);

    auto modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(_ui->deleteModeButton, 0);
    modeGroup->addButton(_ui->cutModeButton, 1);
    modeGroup->addButton(_ui->clipModeButton, 2);
    const auto mode = Preferences::get()->getInt("/tools/eraser/mode", 1);
    modeGroup->button(std::clamp(mode, 0, 2))->setChecked(true);
    QObject::connect(modeGroup, &QButtonGroup::idToggled, this, [](int id, bool checked) {
        if (checked) {
            Preferences::get()->setInt("/tools/eraser/mode", id);
        }
    });

    auto updateModeVisibility = [this](int id) {
        const bool isDelete = id == 0;
        const bool isCut = id == 1;
        _ui->widthLabel->setVisible(!isDelete);
        _ui->widthSpin->setVisible(!isDelete);
        _ui->thinningLabel->setVisible(!isDelete);
        _ui->thinningSpin->setVisible(!isDelete);
        _ui->capRoundingLabel->setVisible(!isDelete);
        _ui->capRoundingSpin->setVisible(!isDelete);
        _ui->tremorLabel->setVisible(!isDelete);
        _ui->tremorSpin->setVisible(!isDelete);
        _ui->massLabel->setVisible(!isDelete);
        _ui->massSpin->setVisible(!isDelete);
        _ui->pressureCheck->setVisible(!isDelete);
        _ui->splitCheck->setVisible(isCut);
    };
    QObject::connect(modeGroup, &QButtonGroup::idClicked, this, updateModeVisibility);
    updateModeVisibility(mode);
}

EraserWidget::~EraserWidget() = default;

void EraserWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

} // namespace Linea::UI
