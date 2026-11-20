// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shared base for operation popups: a grid of QToolButton tiles in a menu.
 */

#ifndef LINEA_UI_OPERATIONS_POPUP_H
#define LINEA_UI_OPERATIONS_POPUP_H

#include <QWidget>

namespace Linea::UI {

/**
 * Base for the toolbar operation popups (path ops, mask ops). Subclasses own
 * a .ui file declaring the tile grid: each tile is a QToolButton carrying an
 * "actionName" dynamic property, plus icon and label.
 * initTiles() applies the shared tile styling and wires every button's click
 * to ActionRegistry::action(name)->trigger().
 */
class OperationsPopup : public QWidget {
    Q_OBJECT

protected:
    explicit OperationsPopup(QWidget* parent = nullptr);

    /** Call after the subclass's setupUi(). */
    void initTiles();

    void hideEvent(QHideEvent* event) override;
};

} // namespace Linea::UI

#endif // LINEA_UI_OPERATIONS_POPUP_H
