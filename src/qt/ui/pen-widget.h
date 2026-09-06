// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PenWidget - Widget with options for the Pen tool.
 */

#ifndef LINEA_UI_PEN_WIDGET_H
#define LINEA_UI_PEN_WIDGET_H

#include <QWidget>

#include <memory>

class QButtonGroup;
class SPDesktop;

namespace Ui {
class PenWidget;
}

namespace Linea::UI {

class PenWidget : public QWidget {
    Q_OBJECT

public:
    explicit PenWidget(QWidget* parent = nullptr);
    ~PenWidget() override;

    void setDesktop(SPDesktop* desktop);

private:
    void setMode(int mode);

    std::unique_ptr<Ui::PenWidget> _ui;
    QButtonGroup* _modeGroup = nullptr;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_PEN_WIDGET_H
