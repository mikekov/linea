// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * MaskOperationsPopup implementation.
 */

#include "mask-operations-popup.h"

#include "ui_mask-operations-popup.h"

namespace Linea::UI {

MaskOperationsPopup::MaskOperationsPopup(QWidget* parent)
    : OperationsPopup(parent)
    , _ui(std::make_unique<Ui::MaskOperationsPopup>()) {
    _ui->setupUi(this);
    initTiles();
}

MaskOperationsPopup::~MaskOperationsPopup() = default;

} // namespace Linea::UI
