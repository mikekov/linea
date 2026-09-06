// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TweakWidget implementation.
 */

#include "tweak-widget.h"

#include <QButtonGroup>
#include <QPushButton>
#include <algorithm>
#include <array>
#include <utility>

#include "preferences.h"
#include "ui_tweak-widget.h"

namespace Linea::UI {

using Inkscape::Preferences;

TweakWidget::TweakWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::TweakWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto configure = [](NumberEdit* spin, const char* path, double value) {
        spin->setRange(1, 100);
        spin->setDecimals(0);
        spin->setValue(100 * Preferences::get()->getDouble(path, value / 100));
        spin->setSingleStep(1);
        QObject::connect(spin, &NumberEdit::valueChanged, spin,
                         [path](double value) { Preferences::get()->setDouble(path, value / 100); });
    };
    configure(_ui->widthSpin, "/tools/tweak/width", 15);
    configure(_ui->forceSpin, "/tools/tweak/force", 20);
    configure(_ui->fidelitySpin, "/tools/tweak/fidelity", 50);

    _ui->pressureCheck->setChecked(Preferences::get()->getBool("/tools/tweak/usepressure", true));
    QObject::connect(_ui->pressureCheck, &QPushButton::toggled, _ui->pressureCheck,
                     [](bool value) { Preferences::get()->setBool("/tools/tweak/usepressure", value); });

    const std::array buttons = {
        _ui->moveButton,         _ui->inOutButton,    _ui->jitterButton,     _ui->scaleButton,
        _ui->rotateButton,       _ui->moreLessButton, _ui->pushButton,       _ui->shrinkGrowButton,
        _ui->attractRepelButton, _ui->roughenButton,  _ui->colorPaintButton, _ui->colorJitterButton,
        _ui->blurButton,
    };
    auto modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        modeGroup->addButton(buttons[i], i);
    }
    auto mode = std::clamp(Preferences::get()->getInt("/tools/tweak/mode", 0), 0, static_cast<int>(buttons.size()) - 1);
    buttons[mode]->setChecked(true);

    auto updateModeVisibility = [this](int id) {
        const bool colorMode = id == 10 || id == 11;
        _ui->fidelityLabel->setVisible(!colorMode);
        _ui->fidelitySpin->setVisible(!colorMode);
        _ui->channelsLabel->setVisible(colorMode);
        _ui->hueButton->setVisible(colorMode);
        _ui->saturationButton->setVisible(colorMode);
        _ui->lightnessButton->setVisible(colorMode);
        _ui->opacityButton->setVisible(colorMode);
    };
    QObject::connect(modeGroup, &QButtonGroup::idClicked, this, [modeGroup, updateModeVisibility](int id) {
        Preferences::get()->setInt("/tools/tweak/mode", id);
        updateModeVisibility(id);
    });
    updateModeVisibility(mode);

    _ui->hueButton->setChecked(Preferences::get()->getBool("/tools/tweak/doh", true));
    _ui->saturationButton->setChecked(Preferences::get()->getBool("/tools/tweak/dos", true));
    _ui->lightnessButton->setChecked(Preferences::get()->getBool("/tools/tweak/dol", true));
    _ui->opacityButton->setChecked(Preferences::get()->getBool("/tools/tweak/doo", true));
    const std::array channelData = {
        std::pair{_ui->hueButton, "/tools/tweak/doh"},
        std::pair{_ui->saturationButton, "/tools/tweak/dos"},
        std::pair{_ui->lightnessButton, "/tools/tweak/dol"},
        std::pair{_ui->opacityButton, "/tools/tweak/doo"},
    };
    for (const auto& [button, path] : channelData) {
        QObject::connect(button, &QPushButton::toggled, button,
                         [path](bool value) { Preferences::get()->setBool(path, value); });
    }
}

TweakWidget::~TweakWidget() = default;

void TweakWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

} // namespace Linea::UI
