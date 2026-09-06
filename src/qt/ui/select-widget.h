// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SelectWidget - Widget with options for the Select tool.
 *
 */

#ifndef LINEA_UI_SELECT_WIDGET_H
#define LINEA_UI_SELECT_WIDGET_H

#include <QWidget>

#include <memory>

class SPDesktop;

namespace Linea::Props {
class Binder;
}

namespace Ui {
class SelectWidget;
}

namespace Linea {

namespace UI {

class SelectWidget : public QWidget {
    Q_OBJECT

public:
    explicit SelectWidget(QWidget* parent = nullptr);
    ~SelectWidget() override;

    void setDesktop(SPDesktop* desktop);
    void bind(Props::Binder& binder);

private:
    std::unique_ptr<Ui::SelectWidget> _ui;
    SPDesktop* _desktop = nullptr;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_SELECT_WIDGET_H
