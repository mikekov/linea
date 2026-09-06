// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ObjectTreeToolbar - Toolbar for the object tree view.
 */

#ifndef LINEA_UI_OBJECT_TREE_TOOLBAR_H
#define LINEA_UI_OBJECT_TREE_TOOLBAR_H

#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class ObjectTreeToolbar;
}
QT_END_NAMESPACE

class SPObject;

namespace Linea::UI {

class ObjectTreeView;
class LayerSelector;

/**
 * Toolbar widget for the object tree.
*
 * Contains:
 * - Move up / move down buttons
 * - Layer selection
 *
 */
class ObjectTreeToolbar : public QWidget {
    Q_OBJECT

public:
    explicit ObjectTreeToolbar(QWidget* parent = nullptr);
    ~ObjectTreeToolbar() override;

    // Connect to an object tree view. The toolbar drives the view's
    // filter text and layers-only mode, and emits move/delete signals
    // based on the view's selection.
    void connectToView(ObjectTreeView* view);

    LayerSelector* getLayerSelector();

Q_SIGNALS:
    void moveUpRequested();
    void moveDownRequested();

private:
    std::unique_ptr<Ui::ObjectTreeToolbar> _ui;
    ObjectTreeView* _view = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_OBJECT_TREE_TOOLBAR_H
