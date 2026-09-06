// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Page widget.
 */

#ifndef LINEA_UI_PAGE_WIDGET_H
#define LINEA_UI_PAGE_WIDGET_H

#include <QWidget>

#include <memory>

namespace Ui {
class PageWidget;
}

namespace Linea::Props {
class Binder;
}

namespace Linea::UI {

class PageWidget : public QWidget {
    Q_OBJECT

public:
    explicit PageWidget(QWidget* parent = nullptr);
    ~PageWidget() override;

    void bind(Props::Binder& binder);

private:
    std::unique_ptr<Ui::PageWidget> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAGE_WIDGET_H
