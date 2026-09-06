// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * About Linea popup widget.
 */

#ifndef LINEA_UI_ABOUT_WIDGET_H
#define LINEA_UI_ABOUT_WIDGET_H

#include <QImage>
#include <QWidget>

#include <memory>

namespace Ui {
class AboutWidget;
}

class QPaintEvent;

namespace Linea::UI {

class AboutWidget : public QWidget {
    Q_OBJECT

public:
    explicit AboutWidget(QWidget* parent = nullptr);
    ~AboutWidget() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::unique_ptr<Ui::AboutWidget> _ui;
    QImage _background;
};

} // namespace Linea::UI

#endif // LINEA_UI_ABOUT_WIDGET_H
