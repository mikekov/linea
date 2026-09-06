// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Connector tool options widget. */

#ifndef LINEA_UI_CONNECTOR_WIDGET_H
#define LINEA_UI_CONNECTOR_WIDGET_H

#include <QWidget>
#include <memory>
#include <sigc++/connection.h>

#include "xml/node-observer.h"

class SPDesktop;

namespace Ui {
class ConnectorWidget;
}

namespace Linea::UI {

class ConnectorWidget : public QWidget, private Inkscape::XML::NodeObserver {
    Q_OBJECT
public:
    explicit ConnectorWidget(QWidget* parent = nullptr);
    ~ConnectorWidget() override;
    void setDesktop(SPDesktop* desktop);

private:
    void orthogonalToggled(bool orthogonal);
    void curvatureChanged(double curvature);
    void spacingChanged(double spacing);
    void selectionChanged();
    void notifyAttributeChanged(Inkscape::XML::Node& node, GQuark name, Inkscape::Util::ptr_shared oldValue,
                                Inkscape::Util::ptr_shared newValue) final;

    std::unique_ptr<Ui::ConnectorWidget> _ui;
    SPDesktop* _desktop = nullptr;
    sigc::connection _selectionChanged;
    bool _updating = false;
    Inkscape::XML::Node* _repr = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_CONNECTOR_WIDGET_H
