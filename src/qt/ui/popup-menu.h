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

class ResizingSeparator;

/**
 * Reusable popup widget with styled container and automatic positioning
 */
class PopupMenu : public QWidget {
    Q_OBJECT

public:
    explicit PopupMenu(QWidget* parent = nullptr);
    ~PopupMenu() override;

    void setContent(QWidget* content);
    void setResizable(bool enabled, QSize minimum, QSize maximum);

    void showBelowWidget(QWidget* widget);
    void showLeftOfWidget(QWidget* widget);
    void showLeftOfWidgetRightEdge(QWidget* widget);
    void showAboveWidget(QWidget* widget);

    // stretches popup to window size before opening
    void showBesideWidget(QWidget* widget);

Q_SIGNALS:
    void popupHidden();
    void resized(QSize size);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    enum class Placement {
        Below,
        Above,
        Left,
        LeftRightEdge,
        Beside,
    };

    void showAt(QWidget* widget, Placement placement);
    void ensurePopupOnScreen(QPoint& pos);

    QVBoxLayout* _layout = nullptr;
    QWidget* _content = nullptr;
    ResizingSeparator* _resizeSeparator = nullptr;
    bool _resizable = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_POPUP_MENU_H
