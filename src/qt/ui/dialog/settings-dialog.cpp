// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief SettingsDialog implementation.
 */

#include "settings-dialog.h"

#include <QGuiApplication>
#include <QRadioButton>
#include <QSlider>
#include <QStyleHints>

#include "ui_settings-dialog.h"

#include "linea-application.h"
#include "preferences.h"
#include "qt/ui/handle-preview.h"
#include "qt/ui/theme.h"

namespace Linea::UI {

namespace {

void applyTheme(ThemeMode mode, QSettings& settings) {
    settings.setValue("dark-theme", static_cast<int>(mode));
    if (mode == ThemeMode::System) {
        // Unpin the scheme so colorScheme() reports the real system state.
        QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
        setApplicationTheme(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark, true);
    } else {
        setApplicationTheme(mode == ThemeMode::Dark, false);
    }
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::SettingsDialog>()) {
    _ui->setupUi(this);

    auto& settings = LINEA_APP.settings();
    switch (static_cast<ThemeMode>(settings.value("dark-theme", 0).toInt())) {
        case ThemeMode::Dark:  _ui->darkRadio->setChecked(true);   break;
        case ThemeMode::Light: _ui->lightRadio->setChecked(true);  break;
        default:               _ui->systemRadio->setChecked(true); break;
    }

    connect(_ui->systemRadio, &QRadioButton::clicked, this, [&settings] { applyTheme(ThemeMode::System, settings); });
    connect(_ui->darkRadio,   &QRadioButton::clicked, this, [&settings] { applyTheme(ThemeMode::Dark,   settings); });
    connect(_ui->lightRadio,  &QRadioButton::clicked, this, [&settings] { applyTheme(ThemeMode::Light,  settings); });

    auto prefs = Inkscape::Preferences::get();
    constexpr auto opengl_pref = "/options/rendering/request_opengl";
    if (prefs->getBool(opengl_pref, false)) {
        _ui->gpuRadio->setChecked(true);
    } else {
        _ui->softwareRadio->setChecked(true);
    }
    connect(_ui->gpuRadio,      &QRadioButton::clicked, this, [prefs] { prefs->setBool(opengl_pref, true); });
    connect(_ui->softwareRadio, &QRadioButton::clicked, this, [prefs] { prefs->setBool(opengl_pref, false); });

    constexpr auto grabsize_pref = "/options/grabsize/value";
    auto grabsize = prefs->getIntLimited(grabsize_pref, 7, 3, 15);
    _ui->handleSizeSlider->setValue(grabsize);
    _ui->handleSizeValue->setText(QString::number(grabsize));
    auto updatePreview = [this] {
        _ui->handlePreview->setPixmap(draw_handles_preview(devicePixelRatioF()));
    };
    updatePreview();
    connect(_ui->handleSizeSlider, &QSlider::valueChanged, this, [this, prefs, updatePreview](int value) {
        prefs->setInt(grabsize_pref, value);
        _ui->handleSizeValue->setText(QString::number(value));
        updatePreview();
    });
}

SettingsDialog::~SettingsDialog() = default;

} // namespace Linea::UI
