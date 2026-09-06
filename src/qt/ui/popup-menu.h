// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PopupMenu — reusable popup widget with styled container
 */

#ifndef LINEA_UI_POPUP_MENU_H
#define LINEA_UI_POPUP_MENU_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Linea::UI {

/**
 * Reusable popup widget with styled container and automatic positioning
 */
class PopupMenu : public QWidget {
    Q_OBJECT

public:
    explicit PopupMenu(QWidget* parent = nullptr);
    ~PopupMenu() override;

    void setContent(QWidget* content);

    void showBelowWidget(QWidget* widget);
    void showLeftOfWidget(QWidget* widget);
    void showLeftOfWidgetRightEdge(QWidget* widget);
    void showAboveWidget(QWidget* widget);

    // stretches popup to window size before opening
    void showBesideWidget(QWidget* widget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void ensurePopupOnScreen(QPoint& pos);

    QVBoxLayout* _layout = nullptr;
    QWidget* _content = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_POPUP_MENU_H
