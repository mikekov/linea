// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PathOperationsPopup implementation.
 */

#include "path-operations-popup.h"

#include "ui_path-operations-popup.h"

namespace Linea::UI {

PathOperationsPopup::PathOperationsPopup(QWidget* parent)
    : OperationsPopup(parent)
    , _ui(std::make_unique<Ui::PathOperationsPopup>()) {
    _ui->setupUi(this);
    initTiles();
}

PathOperationsPopup::~PathOperationsPopup() = default;

} // namespace Linea::UI
