// SPDX-License-Identifier: GPL-2.0-or-later
/** @file PaintbucketWidget implementation. */

#include "paintbucket-widget.h"

#include <QPushButton>

#include <glibmm/i18n.h>

#include "icon-combobox.h"
#include "number-edit.h"
#include "preferences.h"
#include "ui_paintbucket-widget.h"
#include "ui/tools/flood-tool.h"
#include "unit-tracker.h"

namespace Linea::UI {

using Inkscape::Preferences;

PaintbucketWidget::PaintbucketWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PaintbucketWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    _ui->channelCombo->setHeaderType(IconComboBox::LabelOnly);
    const auto& channels = Inkscape::UI::Tools::FloodTool::channel_list;
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        _ui->channelCombo->addRow({}, QString::fromUtf8(_(channels[i])), i);
    }
    _ui->channelCombo->setActiveById(Preferences::get()->getInt("/tools/paintbucket/channels", 0));
    connect(_ui->channelCombo, &IconComboBox::currentChanged, this,
            [](int id) { Preferences::get()->setInt("/tools/paintbucket/channels", id); });

    auto configure = [](NumberEdit* spin, const char* path, double value, double minimum, double maximum, int decimals,
                        double step) {
        spin->setRange(minimum, maximum);
        spin->setDecimals(decimals);
        spin->setSingleStep(step);
        spin->setValue(Preferences::get()->getDouble(path, value));
        connect(spin, &NumberEdit::valueChanged, spin,
                [path](double newValue) { Preferences::get()->setDouble(path, newValue); });
    };
    configure(_ui->thresholdSpin, "/tools/paintbucket/threshold", 5, 0, 100, 0, 1);
    configure(_ui->offsetSpin, "/tools/paintbucket/offset", 0, -10000, 10000, 2, 0.1);

    _tracker = std::make_unique<UnitTracker>(Inkscape::Util::UNIT_TYPE_LINEAR, this);
    _tracker->addSpinBox(_ui->offsetSpin);
    _tracker->attachPopup(_ui->unitsButton);
    auto updateUnitUi = [this](const Inkscape::Util::Unit* unit) {
        if (!unit) return;
        const auto abbr = QString::fromStdString(unit->abbr.raw());
        _ui->offsetSpin->setSuffix(QString(" %1").arg(abbr));
        Preferences::get()->setString("/tools/paintbucket/offsetunits", unit->abbr.raw());
    };
    connect(_tracker.get(), &UnitTracker::unitChanged, this, updateUnitUi);
    const auto storedUnit = Preferences::get()->getString("/tools/paintbucket/offsetunits");
    if (!storedUnit.empty()) {
        _tracker->setActiveUnitByAbbr(storedUnit.c_str());
    } else {
        _tracker->setActiveUnitByAbbr("px");
    }
    updateUnitUi(_tracker->getActiveUnit());

    _ui->autoGapCombo->setHeaderType(IconComboBox::LabelOnly);
    const auto& gaps = Inkscape::UI::Tools::FloodTool::gap_list;
    for (int i = 0; i < static_cast<int>(gaps.size()); ++i) {
        _ui->autoGapCombo->addRow({}, QString::fromUtf8(g_dpgettext2(nullptr, "Flood autogap", gaps[i])), i);
    }
    _ui->autoGapCombo->setActiveById(Preferences::get()->getInt("/tools/paintbucket/autogap", 0));
    connect(_ui->autoGapCombo, &IconComboBox::currentChanged, this,
            [](int id) { Preferences::get()->setInt("/tools/paintbucket/autogap", id); });

    connect(_ui->resetButton, &QPushButton::clicked, this, &PaintbucketWidget::resetDefaults);
}

PaintbucketWidget::~PaintbucketWidget() = default;

void PaintbucketWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void PaintbucketWidget::resetDefaults() {
    _ui->channelCombo->setActiveById(Inkscape::UI::Tools::FLOOD_CHANNELS_RGB);
    _ui->thresholdSpin->setValue(15);
    _ui->offsetSpin->setValue(0);
    _tracker->setActiveUnitByAbbr("px");
    _ui->autoGapCombo->setActiveById(0);
}

} // namespace Linea::UI
