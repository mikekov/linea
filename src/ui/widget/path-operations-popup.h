// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Menu-style list of path operations for the toolbar popup button.
 */

#ifndef LINEA_UI_PATH_OPERATIONS_POPUP_H
#define LINEA_UI_PATH_OPERATIONS_POPUP_H

#include <memory>

#include "operations-popup.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PathOperationsPopup;
}
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Path operations as a grid of icon + label tiles.
 * Each button triggers the corresponding app action via ActionRegistry.
 */
class PathOperationsPopup : public OperationsPopup {
    Q_OBJECT

public:
    explicit PathOperationsPopup(QWidget* parent = nullptr);
    ~PathOperationsPopup() override;

private:
    std::unique_ptr<Ui::PathOperationsPopup> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_PATH_OPERATIONS_POPUP_H
