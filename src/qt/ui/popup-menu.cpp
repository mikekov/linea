// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PopupMenu — reusable popup widget with styled container
 */

#include "popup-menu.h"

#include <QEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace Linea::UI {

PopupMenu::PopupMenu(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    auto outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto mainContainer = new QWidget(this);
    mainContainer->setProperty("class", "popup-menu");
    outerLayout->addWidget(mainContainer);

    _layout = new QVBoxLayout(mainContainer);
    _layout->setContentsMargins(8, 8, 8, 8);
    _layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    installEventFilter(this);
}

PopupMenu::~PopupMenu() = default;

void PopupMenu::setContent(QWidget* content) {
    _content = content;
    if (_content) {
        _layout->addWidget(_content);
    }
}

void PopupMenu::showBelowWidget(QWidget* widget) {
    if (!widget) return;

    show();
    adjustSize();

    QPoint widgetCenter = widget->mapToGlobal(widget->rect().center());
    QPoint popupPos(widgetCenter.x() - width() / 2, widget->mapToGlobal(widget->rect().bottomLeft()).y() + 1);
    ensurePopupOnScreen(popupPos);
    move(popupPos);
}

void PopupMenu::showAboveWidget(QWidget* widget) {
    if (!widget) return;

    show();
    adjustSize();

    // Align the popup's right edge with the widget's right edge,
    // and place the popup's bottom edge just above the widget's top edge.
    QPoint widgetTopRight = widget->mapToGlobal(widget->rect().topRight());
    QPoint popupPos(widgetTopRight.x() - width(), widgetTopRight.y() - height() - 1);
    ensurePopupOnScreen(popupPos);
    move(popupPos);
}

void PopupMenu::showLeftOfWidget(QWidget* widget) {
    if (!widget) return;

    show();
    adjustSize();

    QPoint widgetCenter = widget->mapToGlobal(widget->rect().center());
    QPoint popupPos(widget->mapToGlobal(widget->rect().topLeft()).x() - width() - 1, widgetCenter.y() - height() / 2);
    ensurePopupOnScreen(popupPos);
    move(popupPos);
}

void PopupMenu::showLeftOfWidgetRightEdge(QWidget* widget) {
    if (!widget) return;

    show();
    adjustSize();

    // Like showLeftOfWidget (vertically centered), but anchored to the
    // widget's right edge instead of its left edge.
    QPoint widgetCenter = widget->mapToGlobal(widget->rect().center());
    QPoint popupPos(widget->mapToGlobal(widget->rect().topRight()).x() - width(), widgetCenter.y() - height() / 2);
    ensurePopupOnScreen(popupPos);
    move(popupPos);
}

void PopupMenu::showBesideWidget(QWidget* widget) {
    if (!widget) return;

    show();
    adjustSize();

    // Place the popup to the left of the widget, with its top aligned to
    // the top of the top-level window.
    QPoint widgetTopLeft = widget->mapToGlobal(widget->rect().topLeft());
    int x = widgetTopLeft.x() - width() - 1;

    QWidget* top = widget->window();
    int y = top ? top->mapToGlobal(top->rect().topLeft()).y() : widgetTopLeft.y();

    QPoint popupPos(x, y);
    ensurePopupOnScreen(popupPos);
    move(popupPos);
}

void PopupMenu::ensurePopupOnScreen(QPoint& pos) {
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    if (screen) {
        QRect screenGeometry = screen->availableGeometry();

        // Adjust horizontal position if off-screen
        if (pos.x() < screenGeometry.left()) {
            pos.setX(screenGeometry.left() + 8);
        } else if (pos.x() + width() > screenGeometry.right()) {
            pos.setX(screenGeometry.right() - width() - 8);
        }

        // Adjust vertical position if off-screen
        if (pos.y() < screenGeometry.top()) {
            pos.setY(screenGeometry.top() + 8);
        } else if (pos.y() + height() > screenGeometry.bottom()) {
            pos.setY(screenGeometry.bottom() - height() - 8);
        }
    }
}

bool PopupMenu::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress && watched != this) {
        hide();
        return true;
    }
    if (event->type() == QEvent::LayoutRequest && _content) {
        adjustSize();
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace Linea::UI
