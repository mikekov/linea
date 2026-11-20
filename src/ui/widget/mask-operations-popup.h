// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Menu-style list of clip and mask operations for the toolbar popup button.
 */

#ifndef LINEA_UI_MASK_OPERATIONS_POPUP_H
#define LINEA_UI_MASK_OPERATIONS_POPUP_H

#include <memory>

#include "operations-popup.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MaskOperationsPopup;
}
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Clip and mask operations as a grid of icon + label tiles.
 * Each button triggers the corresponding app action via ActionRegistry.
 */
class MaskOperationsPopup : public OperationsPopup {
    Q_OBJECT

public:
    explicit MaskOperationsPopup(QWidget* parent = nullptr);
    ~MaskOperationsPopup() override;

private:
    std::unique_ptr<Ui::MaskOperationsPopup> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_MASK_OPERATIONS_POPUP_H
