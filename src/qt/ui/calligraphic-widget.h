// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * CalligraphicWidget - Widget with options for the Calligraphic tool.
 */

#ifndef LINEA_UI_CALLIGRAPHIC_WIDGET_H
#define LINEA_UI_CALLIGRAPHIC_WIDGET_H

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class SPDesktop;

namespace Ui {
class CalligraphicWidget;
}

namespace Linea::UI {

class CalligraphicWidget : public QWidget {
    Q_OBJECT

public:
    explicit CalligraphicWidget(QWidget* parent = nullptr);
    ~CalligraphicWidget() override;

    void setDesktop(SPDesktop* desktop);

private:
    void populatePresets();
    void applyPreset(int id);

    std::unique_ptr<Ui::CalligraphicWidget> _ui;
    SPDesktop* _desktop = nullptr;
    std::vector<std::string> _presetPaths;

};

} // namespace Linea::UI

#endif // LINEA_UI_CALLIGRAPHIC_WIDGET_H
