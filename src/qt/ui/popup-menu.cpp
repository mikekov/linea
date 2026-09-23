// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PopupMenu — reusable popup widget with styled container
 */

#include "popup-menu.h"

#include <QEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <glib/gi18n.h>

#include "resizing-separator.h"

namespace Linea::UI {

PopupMenu::PopupMenu(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    auto outerLayout = new QGridLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    auto mainContainer = new QWidget(this);
    mainContainer->setProperty("class", "popup-menu");
    outerLayout->addWidget(mainContainer, 0, 0);

    _layout = new QVBoxLayout(mainContainer);
    _layout->setContentsMargins(8, 8, 8, 8);
    _layout->setSpacing(0);
    _layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    _resizeSeparator = new ResizingSeparator;
    _resizeSeparator->setOrientation(ResizingSeparator::Orientation::Both);
    _resizeSeparator->setFixedSize(10, 10);
    outerLayout->addWidget(_resizeSeparator, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    _resizeSeparator->hide();
    connect(_resizeSeparator, &ResizingSeparator::resized, this, [this](Geom::Point size) {
        Q_EMIT resized(QSize(static_cast<int>(size.x()), static_cast<int>(size.y())));
    });

    installEventFilter(this);
}

PopupMenu::~PopupMenu() = default;

void PopupMenu::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    Q_EMIT popupHidden();
}

void PopupMenu::setContent(QWidget* content) {
    _content = content;
    if (_content) {
        _layout->addWidget(_content);
    }
}

void PopupMenu::setResizable(bool enabled, QSize minimum, QSize maximum) {
    _resizable = enabled;
    if (!enabled) {
        if (_resizeSeparator) {
            _resizeSeparator->hide();
        }
        return;
    }

    _resizeSeparator->resize(this, minimum, maximum);
    _resizeSeparator->raise();
    _resizeSeparator->show();
}

void PopupMenu::showBelowWidget(QWidget* widget) {
    showAt(widget, Placement::Below);
}

void PopupMenu::showAboveWidget(QWidget* widget) {
    showAt(widget, Placement::Above);
}

void PopupMenu::showLeftOfWidget(QWidget* widget) {
    showAt(widget, Placement::Left);
}

void PopupMenu::showLeftOfWidgetRightEdge(QWidget* widget) {
    showAt(widget, Placement::LeftRightEdge);
}

void PopupMenu::showBesideWidget(QWidget* widget) {
    showAt(widget, Placement::Beside);
}

void PopupMenu::showAt(QWidget* widget, Placement placement) {
    if (!widget) {
        return;
    }

    show();
    if (!_resizable) {
        adjustSize();
    }

    QPoint popupPos;
    switch (placement) {
        case Placement::Below: {
            const auto widgetCenter = widget->mapToGlobal(widget->rect().center());
            popupPos = {widgetCenter.x() - width() / 2, widget->mapToGlobal(widget->rect().bottomLeft()).y() + 1};
            break;
        }
        case Placement::Above: {
            // Align the popup's right edge with the widget's right edge,
            // and place the popup's bottom edge just above the widget's top edge.
            const auto widgetTopRight = widget->mapToGlobal(widget->rect().topRight());
            popupPos = {widgetTopRight.x() - width(), widgetTopRight.y() - height() - 1};
            break;
        }
        case Placement::Left: {
            const auto widgetCenter = widget->mapToGlobal(widget->rect().center());
            popupPos = {widget->mapToGlobal(widget->rect().topLeft()).x() - width() - 1,
                        widgetCenter.y() - height() / 2};
            break;
        }
        case Placement::LeftRightEdge: {
            // Like showLeftOfWidget (vertically centered), but anchored to the
            // widget's right edge instead of its left edge.
            const auto widgetCenter = widget->mapToGlobal(widget->rect().center());
            popupPos = {widget->mapToGlobal(widget->rect().topRight()).x() - width(),
                        widgetCenter.y() - height() / 2};
            break;
        }
        case Placement::Beside: {
            // Place the popup to the left of the widget, with its top aligned to
            // the top of the top-level window.
            const auto widgetTopLeft = widget->mapToGlobal(widget->rect().topLeft());
            const auto top = widget->window();
            const int y = top ? top->mapToGlobal(top->rect().topLeft()).y() : widgetTopLeft.y();
            popupPos = {widgetTopLeft.x() - width() - 1, y};
            break;
        }
    }

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
    if (event->type() == QEvent::LayoutRequest && _content && !_resizable) {
        adjustSize();
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace Linea::UI
