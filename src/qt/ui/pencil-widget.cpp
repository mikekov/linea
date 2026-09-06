// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PencilWidget implementation.
 */

#include "pencil-widget.h"

#include <QPushButton>
#include <QSignalBlocker>

#include "preferences.h"
#include "ui_pencil-widget.h"

namespace Linea::UI {

using Inkscape::Preferences;

PencilWidget::PencilWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PencilWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(_ui->pressureCheck, &QPushButton::toggled, this, &PencilWidget::setPressureEnabled);
}

PencilWidget::~PencilWidget() = default;

void PencilWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;

    QSignalBlocker pressureBlocker(_ui->pressureCheck);
    _ui->pressureCheck->setChecked(Preferences::get()->getBool("/tools/freehand/pencil/pressure", false));
}

void PencilWidget::setPressureEnabled(bool enabled) {
    Preferences::get()->setBool("/tools/freehand/pencil/pressure", enabled);
}

} // namespace Linea::UI
