// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PencilWidget - Widget with options for the Pencil tool.
 */

#ifndef LINEA_UI_PENCIL_WIDGET_H
#define LINEA_UI_PENCIL_WIDGET_H

#include <QWidget>

#include <memory>

class QButtonGroup;
class SPDesktop;

namespace Ui {
class PencilWidget;
}

namespace Linea::UI {

class PencilWidget : public QWidget {
    Q_OBJECT

public:
    explicit PencilWidget(QWidget* parent = nullptr);
    ~PencilWidget() override;

    void setDesktop(SPDesktop* desktop);

private:
    void setMode(int mode);
    void setPressureEnabled(bool enabled);
    void setMinPressure(int value);
    void setMaxPressure(int value);

    std::unique_ptr<Ui::PencilWidget> _ui;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_PENCIL_WIDGET_H
