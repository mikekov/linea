// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LeftPanel — the left collapsible panel (object tree + XML tree + search).
 */

#ifndef LINEA_UI_DESKTOP_LEFT_PANEL_H
#define LINEA_UI_DESKTOP_LEFT_PANEL_H

#include <QLineEdit>
#include <sigc++/scoped_connection.h>

#include "ui/operation-blocker.h"
#include "ui/widget/collapsible-panel.h"

class QStackedWidget;
class QSettings;
class SPDesktop;
class SPObject;
class SPGroup;

namespace Inkscape::XML {
class Node;
}

namespace Linea::UI {

class XmlTreeWidget;
class ObjectTreeView;
class ObjectTreeToolbar;
class LayerSelector;
class PanelSwitch;

class LeftPanel : public CollapsiblePanel {
    Q_OBJECT

public:
    explicit LeftPanel(QWidget* parent = nullptr);
    ~LeftPanel() override;

    /// Build the header toolbar. Call after actions are registered.
    void buildHeader();

    ObjectTreeView* objectTreeView() const { return _objectTreeView; }
    XmlTreeWidget* xmlTreeWidget() const { return _xmlTreeWidget; }
    PanelSwitch* switchWidget() const { return _panelSwitch; }

    /// Update content for a new active desktop (or null to clear).
    void setDesktop(SPDesktop* desktop);
    void clear();

    /// Sync the tree selection from the desktop's current selection.
    void updateSelectionFromDesktop();

    void saveSettings(QSettings& settings);
    void restoreSettings(QSettings& settings);

Q_SIGNALS:
    void objectsSelected(std::vector<SPObject*> objects);
    void virtualNodeSelected(int type);
    void xmlNodeSelected(Inkscape::XML::Node* node);
    void panelSwitchChanged(int index);

private:
    void buildUi();
    void connectSignals();

    QStackedWidget* _stackedWidget = nullptr;
    XmlTreeWidget* _xmlTreeWidget = nullptr;
    QWidget* _objectTreeContainer = nullptr;
    ObjectTreeToolbar* _objectTreeToolbar = nullptr;
    ObjectTreeView* _objectTreeView = nullptr;
    LayerSelector* _layerSelector = nullptr;
    PanelSwitch* _panelSwitch = nullptr;
    QLineEdit* _searchEdit = nullptr;

    SPDesktop* _desktop = nullptr;
    OperationBlocker _syncObjectTree;
    OperationBlocker _syncXmlTree;
    sigc::scoped_connection _layerChangedConn;
    sigc::scoped_connection _currentLayerModifiedConn;
};

} // namespace Linea::UI

#endif // LINEA_UI_DESKTOP_LEFT_PANEL_H
