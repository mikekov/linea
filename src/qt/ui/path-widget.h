// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PathWidget — compact path-data action widget.
 */

#ifndef LINEA_UI_PATH_WIDGET_H
#define LINEA_UI_PATH_WIDGET_H

#include <QWidget>
#include <memory>

namespace Ui {
class PathWidget;
}

namespace Linea::Props {
class Binder;
}

class SPDesktop;

namespace Linea::UI {

class PointWidget;
class PopupMenu;

/**
 * Compact path row containing a Path label and an action button.
 */
class PathWidget : public QWidget {
    Q_OBJECT

public:
    explicit PathWidget(QWidget* parent = nullptr);
    ~PathWidget() override;

    void bind(Props::Binder& binder);

private Q_SLOTS:
    void onPathButtonClicked();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updatePointObject();

    std::unique_ptr<Ui::PathWidget> _ui;
    PopupMenu* _popup = nullptr;
    PointWidget* _pointWidget = nullptr;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_PATH_WIDGET_H
