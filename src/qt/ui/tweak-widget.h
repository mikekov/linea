// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TweakWidget - Widget with options for the Tweak tool.
 */

#ifndef LINEA_UI_TWEAK_WIDGET_H
#define LINEA_UI_TWEAK_WIDGET_H

#include <QWidget>
#include <memory>

class SPDesktop;

namespace Ui {
class TweakWidget;
}

namespace Linea::UI {

class TweakWidget : public QWidget {
    Q_OBJECT

public:
    explicit TweakWidget(QWidget* parent = nullptr);
    ~TweakWidget() override;

    void setDesktop(SPDesktop* desktop);

private:
    std::unique_ptr<Ui::TweakWidget> _ui;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_TWEAK_WIDGET_H
