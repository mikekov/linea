// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Spray tool options widget. */
#ifndef LINEA_UI_SPRAY_WIDGET_H
#define LINEA_UI_SPRAY_WIDGET_H
#include <QWidget>
#include <memory>
class SPDesktop;
namespace Ui {
class SprayWidget;
}
namespace Linea::UI {
class SprayWidget : public QWidget {
    Q_OBJECT
public:
    explicit SprayWidget(QWidget* parent = nullptr);
    ~SprayWidget() override;
    void setDesktop(SPDesktop* desktop);

private:
    std::unique_ptr<Ui::SprayWidget> _ui;
    SPDesktop* _desktop = nullptr;
};
} // namespace Linea::UI
#endif // LINEA_UI_SPRAY_WIDGET_H
