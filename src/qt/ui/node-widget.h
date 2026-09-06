// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * NodeWidget - Widget for editing node properties.
 *
 */

#ifndef LINEA_UI_NODE_WIDGET_H
#define LINEA_UI_NODE_WIDGET_H

#include <QWidget>

#include <memory>
#include <sigc++/connection.h>

class SPDesktop;

namespace Inkscape {
namespace UI {
class ControlPointSelection;
namespace Tools { class NodeTool; }
}
}

namespace Ui {
class NodeWidget;
}

namespace Linea {

namespace Props { class Binder; }

namespace UI {

class NodeWidget : public QWidget {
    Q_OBJECT

public:
    explicit NodeWidget(QWidget* parent = nullptr);
    ~NodeWidget() override;

    void setDesktop(SPDesktop* desktop);

    void bind(Props::Binder& binder);

private:
    Inkscape::UI::Tools::NodeTool* getNodeTool() const;

    void edit_join();
    void edit_break();
    void edit_join_segment();
    void edit_delete_segment();
    void edit_cusp();
    void edit_smooth();
    void edit_symmetrical();
    void edit_auto();
    void edit_toline();
    void edit_tocurve();
    void updateNodeControls(Inkscape::UI::ControlPointSelection* selectedNodes);
    void editNodePosition(double value, bool xCoordinate);
    void editNodeDistance(double value);

    std::unique_ptr<Ui::NodeWidget> _ui;
    SPDesktop* _desktop = nullptr;
    sigc::connection _selectionChanged;
    sigc::connection _subselectionChanged;
    bool _updatingNodeControls = false;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_NODE_WIDGET_H
