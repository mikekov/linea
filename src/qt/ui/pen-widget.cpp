// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PenWidget implementation.
 */

#include "pen-widget.h"

#include <QButtonGroup>
#include <QPushButton>

#include "desktop.h"
#include "preferences.h"
#include "ui/tools/pen-tool.h"
#include "ui_pen-widget.h"

namespace Linea::UI {

PenWidget::PenWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PenWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto dummyPolicy = _ui->dummy->sizePolicy();
    dummyPolicy.setRetainSizeWhenHidden(true);
    _ui->dummy->setSizePolicy(dummyPolicy);

    _modeGroup = new QButtonGroup(this);
    _modeGroup->setExclusive(true);
    _modeGroup->addButton(_ui->bezierButton, 0);   // Bezier
    _modeGroup->addButton(_ui->bsplineButton, 2);  // BSpline
    _modeGroup->addButton(_ui->paraxialButton, 4); // Paraxial
    connect(_modeGroup, &QButtonGroup::idClicked, this, &PenWidget::setMode);
}

PenWidget::~PenWidget() = default;

void PenWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;

    auto mode = Preferences::get()->getInt("/tools/freehand/pen/freehand-mode", 0);
    if (mode < 0 || mode >= 5) mode = 0;
    if (auto button = _modeGroup->button(mode)) {
        button->setChecked(true);
    }
}

void PenWidget::setMode(int mode) {
    if (!_desktop || mode < 0 || mode >= 5) return;

    Preferences::get()->setInt("/tools/freehand/pen/freehand-mode", mode);
    if (auto tool = dynamic_cast<Inkscape::UI::Tools::PenTool*>(_desktop->getTool())) {
        tool->setPolylineMode();
    }
}

} // namespace Linea::UI
