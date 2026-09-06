// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * EraserWidget - Widget with options for the Eraser tool.
 */

#ifndef LINEA_UI_ERASER_WIDGET_H
#define LINEA_UI_ERASER_WIDGET_H

#include <QWidget>
#include <memory>

class SPDesktop;

namespace Ui {
class EraserWidget;
}

namespace Linea::UI {

class EraserWidget : public QWidget {
    Q_OBJECT

public:
    explicit EraserWidget(QWidget* parent = nullptr);
    ~EraserWidget() override;

    void setDesktop(SPDesktop* desktop);

private:
    std::unique_ptr<Ui::EraserWidget> _ui;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_ERASER_WIDGET_H
